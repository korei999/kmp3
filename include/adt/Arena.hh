#pragma once

#include "IAllocator.hh"
#include "assert.hh"
#include "utils.hh"
#include "SList.hh"

#if __has_include(<sys/mman.h>)
    #define ADT_FLAT_ARENA_MMAP
    #include <sys/mman.h>
#elif defined _WIN32
    #define ADT_FLAT_ARENA_WIN32
#else
    #warning "FlatArena in not implemented"
#endif

namespace adt
{

struct ArenaStateGuard;

struct Arena : IArena
{
    friend ArenaStateGuard;

    struct DestructorNode
    {
        void** ppObj {};
        void (*pfnDestruct)(Arena* pArena, void** ppObj) {};
    };

    using ListNodeType = SList<DestructorNode>::Node;

    template<typename T>
    struct Ptr : ListNodeType
    {
        T* m_pData {};

        /* */

        Ptr() noexcept = default;

        template<typename ...ARGS>
        Ptr(Arena* pArena, ARGS&&... args)
            : m_pData {pArena->alloc<T>(std::forward<ARGS>(args)...)} {}

        /* */

        explicit operator bool() noexcept { return m_pData != nullptr; }

        T& operator*() noexcept { ADT_ASSERT(m_pData != nullptr, ""); return *m_pData; }
        T* operator->() noexcept { ADT_ASSERT(m_pData != nullptr, ""); return m_pData; }
    };

    /* */

    void* m_pData {};
    isize m_off {};
    isize m_reserved {};
    isize m_commited {};
    void* m_pLastAlloc {};
    isize m_lastAllocSize {};
    SList<DestructorNode> m_lDestructors {}; /* Kill all the 'owned' objects on reset()/freeAll() or state restores. */
    SList<DestructorNode>* m_pTargetList = &m_lDestructors; /* Switch and restore current target on StateGuard changes. */

    /* */

    Arena(isize reserveSize, isize commitSize = getPageSize()) noexcept(false); /* AllocException */
    Arena() noexcept = default;

    /* */

    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override; /* AllocException */

    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override; /* AllocException */

    [[nodiscard]] virtual void* realloc(void* p, usize oldCount, usize newCount, usize mSize) noexcept(false) override; /* AllocException */

    virtual void free(void* ptr) noexcept override;

    [[nodiscard]] virtual constexpr bool doesFree() const noexcept override;
    [[nodiscard]] virtual constexpr bool doesRealloc() const noexcept override;

    virtual void freeAll() noexcept override;

    /* */

    template<typename T>
    void addToDestructList(Ptr<T>* pPtr) noexcept;

    template<typename T>
    void addToDestructList(Ptr<T>* pPtr, void (*pfn)(Arena* pArena, void** ppObj)) noexcept;

    void reset() noexcept;
    void resetDecommit();
    void resetToFirstPage();

protected:
    void destructOwned() noexcept;
    void growIfNeeded(isize newOff);
    void commit(void* p, isize size);
    void decommit(void* p, isize size);
};

struct ArenaState
{
    isize m_off {};
    void* m_pLastAlloc {};
    isize m_lastAllocSize {};
    SList<Arena::DestructorNode>* m_pTargetList {};

    /* */

    void restore(Arena* pArena) noexcept;
};

struct ArenaStateGuard
{
    Arena* m_pArena {};
    SList<Arena::DestructorNode> m_lDestructors {};
    ArenaState m_state {};

    /* */

    ArenaStateGuard(Arena* p) noexcept;
    ~ArenaStateGuard() noexcept;
};

inline void
ArenaState::restore(Arena* pArena) noexcept
{
    pArena->m_off = m_off;
    pArena->m_pLastAlloc = m_pLastAlloc;
    pArena->m_lastAllocSize = m_lastAllocSize;
    pArena->m_pTargetList = m_pTargetList;
}

inline
ArenaStateGuard::ArenaStateGuard(Arena* p) noexcept
    : m_pArena{p},
      m_state{
          .m_off = p->m_off,
          .m_pLastAlloc = p->m_pLastAlloc,
          .m_lastAllocSize = p->m_lastAllocSize,
          .m_pTargetList = p->m_pTargetList
      }
{
    m_pArena->m_pTargetList = &m_lDestructors;
}

inline
ArenaStateGuard::~ArenaStateGuard() noexcept
{
    m_pArena->destructOwned();
    m_state.restore(m_pArena);
}

inline
Arena::Arena(isize reserveSize, isize commitSize)
{
    [[maybe_unused]] int err = 0;

    const isize realReserved = alignUp(reserveSize, getPageSize());

#ifdef ADT_FLAT_ARENA_MMAP
    void* pRes = mmap(nullptr, realReserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pRes == MAP_FAILED) throw AllocException{"mmap() failed"};
#elif defined ADT_FLAT_ARENA_WIN32
    void* pRes = VirtualAlloc(nullptr, realReserved, MEM_RESERVE, PAGE_READWRITE);
    ADT_ALLOC_EXCEPTION_FMT(pRes, "VirtualAlloc() failed to reserve: {}", realReserved);
#else
#endif

    m_pData = pRes;
    m_reserved = realReserved;
    m_pLastAlloc = (void*)~0llu;

    if (commitSize > 0)
    {
        const isize realCommit = alignUp(commitSize, getPageSize());
        commit(m_pData, realCommit);
        m_commited = realCommit;
    }
}

inline void*
Arena::malloc(usize mCount, usize mSize)
{
    const isize realSize = alignUp8(mCount * mSize);
    void* pRet = (void*)((u8*)m_pData + m_off);

    growIfNeeded(m_off + realSize);

    m_pLastAlloc = pRet;
    m_lastAllocSize = realSize;

    return pRet;
}

inline void*
Arena::zalloc(usize mCount, usize mSize)
{
    void* pMem = malloc(mCount, mSize);
    ::memset(pMem, 0, mCount * mSize);
    return pMem;
}

inline void*
Arena::realloc(void* p, usize oldCount, usize newCount, usize mSize)
{
    if (!p) return malloc(newCount, mSize);

    /* bump case */
    if (p == m_pLastAlloc)
    {
        const isize realSize = alignUp8(newCount * mSize);
        isize newOff = (m_off - m_lastAllocSize) + realSize;
        growIfNeeded(newOff);
        m_lastAllocSize = realSize;
        return p;
    }

    if (newCount <= oldCount) return p;

    void* pMem = malloc(newCount, mSize);
    if (p) ::memcpy(pMem, p, oldCount * mSize);
    return pMem;
}

inline void
Arena::free(void*) noexcept
{
    /* noop */
}

inline constexpr bool
Arena::doesFree() const noexcept
{
    return false;
}

inline constexpr bool
Arena::doesRealloc() const noexcept
{
    return true;
}

inline void
Arena::freeAll() noexcept
{
    destructOwned();

#ifdef ADT_FLAT_ARENA_MMAP
    [[maybe_unused]] int err = munmap(m_pData, m_reserved);
    ADT_ASSERT(err != - 1, "munmap: {} ({})", err, strerror(errno));
#elif defined ADT_FLAT_ARENA_WIN32
    VirtualFree(m_pData, 0, MEM_RELEASE);
#else
#endif

    *this = {};
}

template<typename T>
inline void
Arena::addToDestructList(Ptr<T>* pPtr) noexcept
{
    pPtr->data.ppObj = (void**)&pPtr->m_pData;
    pPtr->data.pfnDestruct = [](Arena*, void** ppObj) {
        auto& r = *((T*)*ppObj);
        r.~T();
        *((T**)ppObj) = nullptr;
    };
    m_pTargetList->insert(static_cast<ListNodeType*>(pPtr));
}

template<typename T>
inline void
Arena::addToDestructList(Ptr<T>* pPtr, void (*pfn)(Arena* pArena, void** ppObj)) noexcept
{
    pPtr->data.ppObj = (void**)&pPtr->m_pData;
    pPtr->data.pfnDestruct = pfn;
    m_pTargetList->insert(static_cast<ListNodeType*>(pPtr));
}

inline void
Arena::reset() noexcept
{
    destructOwned();

    m_off = 0;
    m_pLastAlloc = (void*)~0llu;
    m_lastAllocSize = 0;
}

inline void
Arena::resetDecommit()
{
    destructOwned();

    decommit(m_pData, m_commited);

    m_off = 0;
    m_commited = 0;
    m_pLastAlloc = (void*)~0llu;
    m_lastAllocSize = 0;
}

inline void
Arena::resetToFirstPage()
{
    destructOwned();

    const isize pageSize = getPageSize();
    [[maybe_unused]] int err = 0;

    if (m_commited > pageSize)
        decommit((u8*)m_pData + pageSize, m_commited - pageSize);
    else if (m_commited < getPageSize())
        commit((u8*)m_pData + m_commited, pageSize - m_commited);

    m_off = 0;
    m_commited = pageSize;
    m_pLastAlloc = (void*)~0llu;
    m_lastAllocSize = 0;
}

inline void
Arena::destructOwned() noexcept
{
    for (auto e : *m_pTargetList)
        e.pfnDestruct(this, e.ppObj);

    m_pTargetList->m_pHead = nullptr;
}

inline void
Arena::growIfNeeded(isize newOff)
{
    if (newOff > m_commited)
    {
        isize newCommited = utils::max((isize)alignUp(newOff, getPageSize()), m_commited * 2);
        ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(newCommited <= m_reserved, "out of reserved memory, newOff: {}, m_reserved: {}", newCommited, m_reserved);
        commit((u8*)m_pData + m_commited, newCommited - m_commited);
        m_commited = newCommited;
    }

    m_off = newOff;
}

inline void
Arena::commit(void* p, isize size)
{
#ifdef ADT_FLAT_ARENA_MMAP
    [[maybe_unused]] int err = mprotect(p, size, PROT_READ | PROT_WRITE);
    ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(err != - 1, "mprotect: r: {} ({}), size: {}", err, strerror(errno), size);
#elif defined ADT_FLAT_ARENA_WIN32
    ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE), "p: {}, size: {}", p, size);
#else
#endif
}

inline void
Arena::decommit(void* p, isize size)
{
#ifdef ADT_FLAT_ARENA_MMAP
        [[maybe_unused]] int err = mprotect(p, size, PROT_NONE);
        ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(err != - 1, "mprotect: {} ({})", err, strerror(errno));
        err = madvise(p, size, MADV_DONTNEED);
        ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(err != - 1, "madvise: {} ({})", err, strerror(errno));
#elif defined ADT_FLAT_ARENA_WIN32
        ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(VirtualFree(p, size, MEM_DECOMMIT), "");
#else
#endif
}

} /* namespace adt */

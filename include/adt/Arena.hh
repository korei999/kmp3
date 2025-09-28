#pragma once

#include "IAllocator.hh"
#include "assert.hh"
#include "utils.hh"
#include "SList.hh"

#if __has_include(<sys/mman.h>)
    #define ADT_ARENA_MMAP
    #include <sys/mman.h>
#elif defined _WIN32
    #define ADT_ARENA_WIN32
#else
    #warning "Arena in not implemented"
#endif

namespace adt
{

struct ArenaScope;

struct Arena : IArena
{
    friend ArenaScope;

    using PfnDeleter = void(*)(void**);

    struct DeleterNode
    {
        void** ppObj {};
        PfnDeleter pfnDelete {};
    };

    using ListNodeType = SList<DeleterNode>::Node;

    template<typename T>
    struct Ptr : protected ListNodeType
    {
        T* m_pData {};

        /* */

        Ptr() noexcept = default;

        template<typename ...ARGS>
        Ptr(Arena* pArena, ARGS&&... args)
            : ListNodeType{nullptr, {(void**)this, (PfnDeleter)nullptrDeleter}},
              m_pData {pArena->alloc<T>(std::forward<ARGS>(args)...)}
        {
            pArena->m_pLCurrentDeleters->insert(static_cast<ListNodeType*>(this));
        }

        template<typename ...ARGS>
        Ptr(void (*pfn)(Ptr*), Arena* pArena, ARGS&&... args)
            : ListNodeType{nullptr, {(void**)this, (PfnDeleter)pfn}},
              m_pData {pArena->alloc<T>(std::forward<ARGS>(args)...)}
        {
            pArena->m_pLCurrentDeleters->insert(static_cast<ListNodeType*>(this));
        }

        /* */

        static void
        nullptrDeleter(Ptr* pPtr) noexcept
        {
            utils::destruct(pPtr->m_pData);
            pPtr->m_pData = nullptr;
        };

        static void
        simpleDeleter(Ptr* pPtr) noexcept
        {
            utils::destruct(pPtr->m_pData);
        };

        /* */

        explicit operator bool() const noexcept { return m_pData != nullptr; }

        T& operator*() noexcept { ADT_ASSERT(m_pData != nullptr, ""); return *m_pData; }
        const T& operator*() const noexcept { ADT_ASSERT(m_pData != nullptr, ""); return *m_pData; }

        T* operator->() noexcept { ADT_ASSERT(m_pData != nullptr, ""); return m_pData; }
        const T* operator->() const noexcept { ADT_ASSERT(m_pData != nullptr, ""); return m_pData; }
    };

    static constexpr u64 INVALID_PTR = ~0llu;

    /* */

    void* m_pData {};
    isize m_pos {};
    isize m_reserved {};
    isize m_commited {};
    void* m_pLastAlloc {};
    isize m_lastAllocSize {};
    SList<DeleterNode> m_lDeleters {}; /* Run deleters on reset()/freeAll() or state restorations. */
    SList<DeleterNode>* m_pLCurrentDeleters {}; /* Switch and restore current list on ArenaScope changes. */

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

    template<typename T, typename ...ARGS> void initPtr(Ptr<T>* pPtr, ARGS&&... args);
    template<typename T, typename ...ARGS> void initPtr(Ptr<T>* pPtr, void (*pfn)(Arena*, Ptr<T>*), ARGS&&... args);

    void reset() noexcept;
    void resetDecommit();
    void resetToPage(isize nthPage);
    isize memoryUsed() const noexcept { return m_pos; }
    isize memoryReserved() const noexcept { return m_reserved; }
    isize memoryCommited() const noexcept { return m_commited; }

protected:
    /* BUG: asan sees it as stack-use-after-scope when running a deleter after variable's scope closes (its fine just ignore). */
    ADT_NO_UB void runDeleters() noexcept;

    void growIfNeeded(isize newPos);
    void commit(void* p, isize size);
    void decommit(void* p, isize size);
};

/* Capture current state to restore it later with restore(). */
struct ArenaState
{
    isize m_pos {};
    void* m_pLastAlloc {};
    isize m_lastAllocSize {};
    SList<Arena::DeleterNode>* m_pLCurrentDeleters {};

    /* */

    void restore(Arena* pArena) noexcept;
};

struct ArenaScope
{
    Arena* m_pArena {};
    SList<Arena::DeleterNode> m_lDeleters {};
    ArenaState m_state {};

    /* */

    ArenaScope(Arena* p) noexcept;
    ~ArenaScope() noexcept;
};

inline void
ArenaState::restore(Arena* pArena) noexcept
{
    ADT_ASAN_POISON((u8*)pArena->m_pData + pArena->m_pos, pArena->m_pos - m_pos);
    pArena->m_pos = m_pos;
    pArena->m_pLastAlloc = m_pLastAlloc;
    pArena->m_lastAllocSize = m_lastAllocSize;
    pArena->m_pLCurrentDeleters = m_pLCurrentDeleters;
}

inline
ArenaScope::ArenaScope(Arena* p) noexcept
    : m_pArena{p},
      m_state{
          .m_pos = p->m_pos,
          .m_pLastAlloc = p->m_pLastAlloc,
          .m_lastAllocSize = p->m_lastAllocSize,
          .m_pLCurrentDeleters = p->m_pLCurrentDeleters
      }
{
    m_pArena->m_pLCurrentDeleters = &m_lDeleters;
}

inline
ArenaScope::~ArenaScope() noexcept
{
    m_pArena->runDeleters();
    m_state.restore(m_pArena);
}

inline
Arena::Arena(isize reserveSize, isize commitSize)
    : m_pLCurrentDeleters{&m_lDeleters}
{
    [[maybe_unused]] int err = 0;

    const isize realReserved = alignUpPO2(reserveSize, getPageSize());

#ifdef ADT_ARENA_MMAP
    void* pRes = mmap(nullptr, realReserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pRes == MAP_FAILED) throw AllocException{"mmap() failed"};
#elif defined ADT_ARENA_WIN32
    void* pRes = VirtualAlloc(nullptr, realReserved, MEM_RESERVE, PAGE_READWRITE);
    ADT_ALLOC_EXCEPTION_FMT(pRes, "VirtualAlloc() failed to reserve: {}", realReserved);
#else
#endif

    m_pData = pRes;
    m_reserved = realReserved;
    m_pLastAlloc = (void*)INVALID_PTR;

    ADT_ASAN_POISON(m_pData, realReserved);

    if (commitSize > 0)
    {
        const isize realCommit = alignUpPO2(commitSize, getPageSize());
        commit(m_pData, realCommit);
        m_commited = realCommit;
    }
}

inline void*
Arena::malloc(usize mCount, usize mSize)
{
    const isize realSize = alignUp8(mCount * mSize);
    void* pRet = (void*)((u8*)m_pData + m_pos);

    growIfNeeded(m_pos + realSize);

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
        const isize newPos = (m_pos - m_lastAllocSize) + realSize;
        growIfNeeded(newPos);
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
    runDeleters();

#ifdef ADT_ARENA_MMAP
    [[maybe_unused]] int err = munmap(m_pData, m_reserved);
    ADT_ASSERT(err != - 1, "munmap: {} ({})", err, strerror(errno));
#elif defined ADT_ARENA_WIN32
    VirtualFree(m_pData, 0, MEM_RELEASE);
#else
#endif

    ADT_ASAN_UNPOISON(m_pData, m_reserved);
    *this = {};
}

template<typename T, typename ...ARGS>
inline void
Arena::initPtr(Ptr<T>* pPtr, ARGS&&... args)
{
    new(pPtr) Arena::Ptr<T> {this, std::forward<ARGS>(args)...};
}

template<typename T, typename ...ARGS>
inline void
Arena::initPtr(Ptr<T>* pPtr, void (*pfn)(Arena*, Ptr<T>*), ARGS&&... args)
{
    new(pPtr) Arena::Ptr<T> {this, pfn, std::forward<ARGS>(args)...};
}

inline void
Arena::reset() noexcept
{
    runDeleters();

    ADT_ASAN_POISON(m_pData, m_pos);

    m_pos = 0;
    m_pLastAlloc = (void*)INVALID_PTR;
    m_lastAllocSize = 0;
}

inline void
Arena::resetDecommit()
{
    runDeleters();

    decommit(m_pData, m_commited);

    ADT_ASAN_POISON(m_pData, m_pos);

    m_pos = 0;
    m_commited = 0;
    m_pLastAlloc = (void*)INVALID_PTR;
    m_lastAllocSize = 0;
}

inline void
Arena::resetToPage(isize nthPage)
{
    const isize commitSize = getPageSize() * nthPage;
    ADT_ALLOC_EXCEPTION_FMT(commitSize <= m_reserved, "commitSize: {}, m_reserved: {}", commitSize, m_reserved);

    runDeleters();

    if (m_commited > commitSize)
        decommit((u8*)m_pData + commitSize, m_commited - commitSize);
    else if (m_commited < commitSize)
        commit((u8*)m_pData + m_commited, commitSize - m_commited);

    ADT_ASAN_POISON(m_pData, m_reserved);

    m_pos = 0;
    m_commited = commitSize;
    m_pLastAlloc = (void*)INVALID_PTR;
    m_lastAllocSize = 0;
}

inline void
Arena::runDeleters() noexcept
{
    for (auto e : *m_pLCurrentDeleters)
        e.pfnDelete(e.ppObj);

    m_pLCurrentDeleters->m_pHead = nullptr;
}

inline void
Arena::growIfNeeded(isize newPos)
{
    if (newPos > m_commited)
    {
        const isize newCommited = utils::max((isize)alignUpPO2(newPos, getPageSize()), m_commited * 2);
        ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(newCommited <= m_reserved, "out of reserved memory, newPos: {}, m_reserved: {}", newCommited, m_reserved);
        commit((u8*)m_pData + m_commited, newCommited - m_commited);
        m_commited = newCommited;
    }

    ADT_ASAN_UNPOISON((u8*)m_pData + m_pos, newPos - m_pos);
    m_pos = newPos;
}

inline void
Arena::commit(void* p, isize size)
{
#ifdef ADT_ARENA_MMAP
    [[maybe_unused]] int err = mprotect(p, size, PROT_READ | PROT_WRITE);
    ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(err != - 1, "mprotect: r: {} ({}), size: {}", err, strerror(errno), size);
#elif defined ADT_ARENA_WIN32
    ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE), "p: {}, size: {}", p, size);
#else
#endif
}

inline void
Arena::decommit(void* p, isize size)
{
#ifdef ADT_ARENA_MMAP
        [[maybe_unused]] int err = mprotect(p, size, PROT_NONE);
        ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(err != - 1, "mprotect: {} ({})", err, strerror(errno));
        err = madvise(p, size, MADV_DONTNEED);
        ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(err != - 1, "madvise: {} ({})", err, strerror(errno));
#elif defined ADT_ARENA_WIN32
        ADT_ALLOC_EXCEPTION_UNLIKELY_FMT(VirtualFree(p, size, MEM_DECOMMIT), "");
#else
#endif
}

namespace print
{

template<typename T>
inline isize
format(Context* pCtx, FormatArgs fmtArgs, const Arena::Ptr<T>& x)
{
    if (x) return format(pCtx, fmtArgs, *x);
    else return format(pCtx, fmtArgs, "null");
}

} /* namespace print */

} /* namespace adt */

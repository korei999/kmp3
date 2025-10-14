#pragma once

#include "IArena.hh"

#if __has_include(<sys/mman.h>)
    #define ADT_ARENA_MMAP
    #include <sys/mman.h>
#elif defined _WIN32
    #define ADT_ARENA_WIN32
#else
    #error "Arena is not implemented"
#endif

namespace adt
{

struct ArenaScope;

/* Reserve/commit style linear allocator. */
struct Arena : IArena
{
    static constexpr u64 INVALID_PTR = ~0llu;

    /* */

    void* m_pData {};
    isize m_pos {};
    isize m_reserved {};
    isize m_commited {};
    void* m_pLastAlloc {};

    /* */

    Arena(isize reserveSize, isize commitSize = getPageSize()) noexcept(false); /* AllocException */
    Arena() noexcept = default;

    /* */

    [[nodiscard]] virtual void* malloc(usize nBytes) noexcept(false) override; /* AllocException */
    [[nodiscard]] virtual void* zalloc(usize nBytes) noexcept(false) override; /* AllocException */
    [[nodiscard]] virtual void* realloc(void* p, usize oldNBytes, usize newNBytes) noexcept(false) override; /* AllocException */
    virtual void free(void* ptr, usize nBytes) noexcept override;

    [[nodiscard]] virtual constexpr bool doesFree() const noexcept override { return false; }
    [[nodiscard]] virtual constexpr bool doesRealloc() const noexcept override { return true; }

    virtual void freeAll() noexcept override;
    [[nodiscard]] virtual IScope restoreAfterScope() noexcept override;
    virtual usize memoryUsed() const noexcept override { return m_pos; }

    /* */

    template<typename T, typename ...ARGS> void initPtr(Ptr<T>* pPtr, ARGS&&... args);
    template<typename T, typename ...ARGS> void initPtr(Ptr<T>* pPtr, void (*pfn)(Arena*, Ptr<T>*), ARGS&&... args);

    void reset() noexcept;
    void resetDecommit();
    void resetToPage(isize nthPage);
    isize memoryReserved() const noexcept { return m_reserved; }
    isize memoryCommited() const noexcept { return m_commited; }

protected:
    void growIfNeeded(isize newPos);
    void commit(void* p, isize size);
    void decommit(void* p, isize size);
};

/* Capture current state to restore it later with restore(). */
struct ArenaState
{
    Arena* m_pArena {};
    isize m_pos {};
    void* m_pLastAlloc {};
    SList<Arena::DeleterNode>* m_pLCurrentDeleters {};

    /* */

    ArenaState() = default;
    explicit ArenaState(Arena* p) noexcept;

    /* */

    void restore() noexcept;
};

struct ArenaScope : IArena::IScopeDestructor
{
    ArenaState m_state {};
    SList<Arena::DeleterNode> m_lDeleters {};

    /* */

    explicit ArenaScope(const ArenaState& state) noexcept;
    explicit ArenaScope(Arena* pArena) noexcept;

    virtual ~ArenaScope() noexcept override;
};

template<>
struct IArena::Scope<Arena> final : ArenaScope
{
    using ArenaScope::ArenaScope;
};

inline
ArenaScope::ArenaScope(const ArenaState& state) noexcept
    : m_state{state}
{
    m_state.m_pArena->m_pLCurrentDeleters = &m_lDeleters;
}

inline
ArenaScope::ArenaScope(Arena* pArena) noexcept
    : m_state{pArena}
{
    m_state.m_pArena->m_pLCurrentDeleters = &m_lDeleters;
}

inline void
ArenaState::restore() noexcept
{
    ADT_ASAN_POISON((u8*)m_pArena->m_pData + m_pArena->m_pos, m_pArena->m_pos - m_pos);
    m_pArena->m_pos = m_pos;
    m_pArena->m_pLastAlloc = m_pLastAlloc;
    m_pArena->m_pLCurrentDeleters = m_pLCurrentDeleters;
}

inline
ArenaState::ArenaState(Arena* p) noexcept
    : m_pArena{p},
      m_pos{p->m_pos},
      m_pLastAlloc{p->m_pLastAlloc},
      m_pLCurrentDeleters{p->m_pLCurrentDeleters}
{
}

inline
ArenaScope::~ArenaScope() noexcept
{
    m_state.m_pArena->runDeleters();
    m_state.restore();
}

inline
Arena::Arena(isize reserveSize, isize commitSize)
    : IArena{INIT}
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
Arena::malloc(usize nBytes)
{
    const isize realSize = alignUp8(nBytes);
    void* pRet = (void*)((u8*)m_pData + m_pos);

    growIfNeeded(m_pos + realSize);

    m_pLastAlloc = pRet;

    return pRet;
}

inline void*
Arena::zalloc(usize nBytes)
{
    void* pMem = malloc(nBytes);
    ::memset(pMem, 0, nBytes);
    return pMem;
}

inline void*
Arena::realloc(void* p, usize oldNBytes, usize newNBytes)
{
    if (!p) return malloc(newNBytes);

    /* bump case */
    if (p == m_pLastAlloc)
    {
        const isize realSize = alignUp8(newNBytes);
        const isize newPos = ((isize)m_pLastAlloc - (isize)m_pData) + realSize;
        growIfNeeded(newPos);
        return p;
    }

    if (newNBytes <= oldNBytes) return p;

    void* pMem = malloc(newNBytes);
    if (p) ::memcpy(pMem, p, oldNBytes);
    return pMem;
}

inline void
Arena::free(void*, usize) noexcept
{
    /* noop */
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

inline IArena::IScope
Arena::restoreAfterScope() noexcept
{
    ArenaState state {this};
    return alloc<ArenaScope>(state);
}

template<typename T, typename ...ARGS>
inline void
Arena::initPtr(Ptr<T>* pPtr, ARGS&&... args)
{
    new (pPtr) Arena::Ptr<T> {this, std::forward<ARGS>(args)...};
}

template<typename T, typename ...ARGS>
inline void
Arena::initPtr(Ptr<T>* pPtr, void (*pfn)(Arena*, Ptr<T>*), ARGS&&... args)
{
    new (pPtr) Arena::Ptr<T> {this, pfn, std::forward<ARGS>(args)...};
}

inline void
Arena::reset() noexcept
{
    runDeleters();

    ADT_ASAN_POISON(m_pData, m_pos);

    m_pos = 0;
    m_pLastAlloc = (void*)INVALID_PTR;
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

#if ADT_ASAN
    if (newPos > m_pos)
        ADT_ASAN_UNPOISON((u8*)m_pData + m_pos, newPos - m_pos);
    else if (newPos < m_pos)
        ADT_ASAN_POISON((u8*)m_pData + newPos, m_pos - newPos);
#endif

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

} /* namespace adt */

#pragma once

#include "IArena.hh"

namespace adt
{

/* Like Arena, but uses list chained memory blocks instead of reserve/commit. */
struct ArenaList : public IArena
{
    struct Block
    {
        Block* pNext {};
        usize size {}; /* excluding sizeof(ArenaBlock) */
        usize nBytesOccupied {};
        u8* pLastAlloc {};
        usize lastAllocSize {};
        u8 pMem[];
    };

    /* */

    usize m_defaultCapacity {};
    IAllocator* m_pBackAlloc {};
#ifndef NDEBUG
    std::source_location m_loc {};
#endif
    Block* m_pBlocks {};

    /* */

    ArenaList() = default;
    ArenaList(
        usize capacity,
        IAllocator* pBackingAlloc = Gpa::inst()
#ifndef NDEBUG
        , std::source_location _DONT_USE_loc = std::source_location::current()
#endif
    ) noexcept(false)
        : IArena{INIT},
          m_defaultCapacity(alignUp8(capacity)),
          m_pBackAlloc(pBackingAlloc),
#ifndef NDEBUG
          m_loc {_DONT_USE_loc},
#endif
          m_pBlocks(allocBlock(m_defaultCapacity))
    {}

    /* */

    [[nodiscard]] virtual void* malloc(usize nBytes) noexcept(false) override;
    [[nodiscard]] virtual void* zalloc(usize nBytes) noexcept(false) override;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldNBytes, usize newNBytes) noexcept(false) override;
    virtual void free(void* ptr, usize nBytes) noexcept override;

    [[nodiscard]] virtual constexpr bool doesFree() const noexcept override { return false; }
    [[nodiscard]] virtual constexpr bool doesRealloc() const noexcept override { return true; }

    virtual void freeAll() noexcept override;
    [[nodiscard]] virtual IScope restoreAfterScope() noexcept override;
    virtual usize memoryUsed() const noexcept override;

    /* */

    void reset() noexcept;
    void shrinkToFirstBlock() noexcept;

    /* */

protected:
    [[nodiscard]] inline Block* allocBlock(usize size);
    [[nodiscard]] inline Block* prependBlock(usize size);
    [[nodiscard]] inline Block* findFittingBlock(usize size);
    [[nodiscard]] inline Block* findBlockFromPtr(u8* ptr);
};

/* We track only last block, and free() all new blocks if they were created. */
struct ArenaListState
{
    ArenaList* m_pArena {};
    ArenaList::Block* m_pLastBlock {};
    usize m_nBytesOccupied {};
    u8* m_pLastAlloc {};
    usize m_lastAllocSize {};

    /* */

    ArenaListState() = default;
    ArenaListState(ArenaList* pArena) noexcept;

    /* */

    void restore() noexcept;
};

struct ArenaListScope : IArena::IScopeDestructor
{
    ArenaListState m_state {};
    SList<ArenaList::DeleterNode> m_lDeleters {};

    /* */

    ArenaListScope(ArenaList* p) noexcept : m_state{p} { p->m_pLCurrentDeleters = &m_lDeleters; }
    ArenaListScope(const ArenaListState& state) noexcept : m_state{state} { m_state.m_pArena->m_pLCurrentDeleters = &m_lDeleters; }

    virtual ~ArenaListScope() noexcept override;
};

template<>
struct IArena::Scope<ArenaList> : ArenaListScope
{
    using ArenaListScope::ArenaListScope;
};

inline
ArenaListScope::~ArenaListScope() noexcept
{
    m_state.m_pArena->runDeleters();
    m_state.restore();
}

inline
ArenaListState::ArenaListState(ArenaList* pArena) noexcept
    : m_pArena{pArena},
      m_pLastBlock{pArena->m_pBlocks},
      m_nBytesOccupied{m_pLastBlock->nBytesOccupied},
      m_pLastAlloc{m_pLastBlock->pLastAlloc},
      m_lastAllocSize{m_pLastBlock->lastAllocSize}
{
}

inline void
ArenaListState::restore() noexcept
{
    m_pLastBlock->nBytesOccupied = m_nBytesOccupied;
    m_pLastBlock->pLastAlloc = m_pLastAlloc;
    m_pLastBlock->lastAllocSize = m_lastAllocSize;

    auto* it = m_pArena->m_pBlocks;
    while (it != m_pLastBlock)
    {
#if defined ADT_DBG_MEMORY && !defined NDEBUG
        LogDebug("[Arena: {}, {}, {}]: deallocating block of size {} on state restoration\n",
            print::shorterSourcePath(m_pArena->m_loc.file_name()),
            m_pArena->m_loc.function_name(),
            m_pArena->m_loc.line(),
            it->size
        );
#endif
        auto* next = it->pNext;
        m_pArena->m_pBackAlloc->free(it, it->size + sizeof(ArenaList::Block));
        it = next;
    }

    m_pLastBlock->pNext = nullptr;
    m_pArena->m_pBlocks = m_pLastBlock;
}

inline ArenaList::Block*
ArenaList::findBlockFromPtr(u8* ptr)
{
    auto* it = m_pBlocks;
    while (it)
    {
        if (ptr >= it->pMem && ptr < &it->pMem[it->size])
            return it;

        it = it->pNext;
    }

    return nullptr;
}

inline ArenaList::Block*
ArenaList::findFittingBlock(usize size)
{
    auto* it = m_pBlocks;
    while (it)
    {
        if (size < it->size - it->nBytesOccupied)
            return it;

        it = it->pNext;
    }

    return nullptr;
}

inline ArenaList::Block*
ArenaList::allocBlock(usize size)
{
    ADT_ASSERT(m_pBackAlloc, "uninitialized: m_pBackAlloc == nullptr");

    /* NOTE: m_pBackAlloc can throw here */
    Block* pBlock = static_cast<Block*>(m_pBackAlloc->zalloc(size + sizeof(*pBlock)));

#if defined ADT_DBG_MEMORY && !defined NDEBUG
    LogDebug("[Arena: {}, {}]: new block of size: {}\n",
        print::shorterSourcePath(m_loc.file_name()), m_loc.line(), size
    );
#endif

    pBlock->size = size;
    pBlock->pLastAlloc = pBlock->pMem;

    return pBlock;
}

inline ArenaList::Block*
ArenaList::prependBlock(usize size)
{
    auto* pNew = allocBlock(size);
    pNew->pNext = m_pBlocks;
    m_pBlocks = pNew;

    return pNew;
}

inline void*
ArenaList::malloc(usize nBytes)
{
    usize realSize = alignUp8(nBytes);
    auto* pBlock = findFittingBlock(realSize);

#if defined ADT_DBG_MEMORY && !defined NDEBUG
    if (m_defaultCapacity <= realSize)
        LogDebug("[Arena: {}, {}]: allocating more than defaultCapacity ({}, {})\n",
            print::shorterSourcePath(m_loc.file_name()), m_loc.line(), m_defaultCapacity, realSize
        );
#endif

    if (!pBlock) pBlock = prependBlock(utils::max(m_defaultCapacity, usize(realSize*1.33)));

    auto* pRet = pBlock->pMem + pBlock->nBytesOccupied;
    ADT_ASSERT(pRet == pBlock->pLastAlloc + pBlock->lastAllocSize, " ");

    pBlock->nBytesOccupied += realSize;
    pBlock->pLastAlloc = pRet;
    pBlock->lastAllocSize = realSize;

    return pRet;
}

inline void*
ArenaList::zalloc(usize nBytes)
{
    auto* p = malloc(nBytes);
    memset(p, 0, alignUp8(nBytes));
    return p;
}

inline void*
ArenaList::realloc(void* ptr, usize oldNBytes, usize newNBytes)
{
    if (!ptr) return malloc(newNBytes);

    const usize realSize = alignUp8(newNBytes);

    auto* pBlock = findBlockFromPtr(static_cast<u8*>(ptr));
    if (!pBlock) throw AllocException("pointer doesn't belong to this arena");

    if (ptr == pBlock->pLastAlloc &&
        pBlock->pLastAlloc + realSize < pBlock->pMem + pBlock->size) /* bump case */
    {
        pBlock->nBytesOccupied -= pBlock->lastAllocSize;
        pBlock->nBytesOccupied += realSize;
        pBlock->lastAllocSize = realSize;

        return ptr;
    }
    else
    {
        if (newNBytes <= oldNBytes) return ptr;

        auto* pRet = malloc(newNBytes);
        memcpy(pRet, ptr, oldNBytes);

        return pRet;
    }
}

inline void
ArenaList::free(void*, usize) noexcept
{
    /* noop */
}

inline void
ArenaList::freeAll() noexcept
{
    auto* it = m_pBlocks;
    while (it)
    {
        auto* next = it->pNext;
        m_pBackAlloc->free(it, it->size + sizeof(Block));
        it = next;
    }
    m_pBlocks = nullptr;
}

inline IArena::IScope
ArenaList::restoreAfterScope() noexcept
{
    ArenaListState state {this};
    return alloc<ArenaListScope>(state);
}

inline void
ArenaList::reset() noexcept
{
    auto* it = m_pBlocks;
    while (it)
    {
        it->nBytesOccupied = 0;
        it->lastAllocSize = 0;
        it->pLastAlloc = it->pMem;

        it = it->pNext;
    }
}

inline void
ArenaList::shrinkToFirstBlock() noexcept
{
    auto* it = m_pBlocks;
    if (!it) return;

    while (it->pNext)
    {
#if defined ADT_DBG_MEMORY && !defined NDEBUG
        LogDebug("[Arena: {}, {}]: shrinking {} sized block\n",
            print::shorterSourcePath(m_loc.file_name()), m_loc.line(), it->size
        );
#endif
        auto* next = it->pNext;
        m_pBackAlloc->free(it, it->size + sizeof(Block));
        it = next;
    }
    m_pBlocks = it;
}

inline usize
ArenaList::memoryUsed() const noexcept
{
    isize total = 0;
    auto* it = m_pBlocks;
    while (it)
    {
        total += it->nBytesOccupied;
        it = it->pNext;
    }

    return total;
}

} /* namespace adt */

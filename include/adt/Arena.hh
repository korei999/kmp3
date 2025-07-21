#pragma once

#include "StdAllocator.hh"
#include "utils.hh"

#include <cstring>

namespace adt
{

/* fast region based allocator, only freeAll() free's memory, free() does nothing */
struct Arena : public IArena
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

    Arena() = default;

    Arena(
        usize capacity,
        IAllocator* pBackingAlloc = StdAllocator::inst()
#ifndef NDEBUG
        , std::source_location _DONT_USE_loc = std::source_location::current()
#endif
    ) noexcept(false)
        : m_defaultCapacity(alignUp8(capacity)),
          m_pBackAlloc(pBackingAlloc),
#ifndef NDEBUG
          m_loc {_DONT_USE_loc},
#endif
          m_pBlocks(allocBlock(m_defaultCapacity))
    {}

    /* */

    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false) override final;
    virtual void free(void* ptr) noexcept override final;
    virtual void freeAll() noexcept override final;
    void reset() noexcept;

    void shrinkToFirstBlock() noexcept;
    isize nBytesOccupied() const noexcept;

    /* */

protected:
    [[nodiscard]] inline Block* allocBlock(usize size);
    [[nodiscard]] inline Block* prependBlock(usize size);
    [[nodiscard]] inline Block* findFittingBlock(usize size);
    [[nodiscard]] inline Block* findBlockFromPtr(u8* ptr);
};

inline Arena::Block*
Arena::findBlockFromPtr(u8* ptr)
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

inline Arena::Block*
Arena::findFittingBlock(usize size)
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

inline Arena::Block*
Arena::allocBlock(usize size)
{
    ADT_ASSERT(m_pBackAlloc, "uninitialized: m_pBackAlloc == nullptr");

    /* NOTE: m_pBackAlloc can throw here */
    Block* pBlock = static_cast<Block*>(m_pBackAlloc->zalloc(1, size + sizeof(*pBlock)));

#if defined ADT_DBG_MEMORY && !defined NDEBUG
    print::err("[Arena: {}, {}, {}]: new block of size: {}\n",
        print::stripSourcePath(m_loc.file_name()), m_loc.function_name(), m_loc.line(), size
    );
#endif

    pBlock->size = size;
    pBlock->pLastAlloc = pBlock->pMem;

    return pBlock;
}

inline Arena::Block*
Arena::prependBlock(usize size)
{
    auto* pNew = allocBlock(size);
    pNew->pNext = m_pBlocks;
    m_pBlocks = pNew;

    return pNew;
}

inline void*
Arena::malloc(usize mCount, usize mSize)
{
    usize realSize = alignUp8(mCount * mSize);
    auto* pBlock = findFittingBlock(realSize);

#if defined ADT_DBG_MEMORY && !defined NDEBUG
    if (m_defaultCapacity <= realSize)
        print::err("[Arena: {}, {}, {}]: allocating more than defaultCapacity ({}, {})\n",
            print::stripSourcePath(m_loc.file_name()), m_loc.function_name(), m_loc.line(), m_defaultCapacity, realSize
        );
#endif

    if (!pBlock) pBlock = prependBlock(utils::max(m_defaultCapacity, usize(realSize*1.3)));

    auto* pRet = pBlock->pMem + pBlock->nBytesOccupied;
    ADT_ASSERT(pRet == pBlock->pLastAlloc + pBlock->lastAllocSize, " ");

    pBlock->nBytesOccupied += realSize;
    pBlock->pLastAlloc = pRet;
    pBlock->lastAllocSize = realSize;

    return pRet;
}

inline void*
Arena::zalloc(usize mCount, usize mSize)
{
    auto* p = malloc(mCount, mSize);
    memset(p, 0, alignUp8(mCount * mSize));
    return p;
}

inline void*
Arena::realloc(void* ptr, usize oldCount, usize mCount, usize mSize)
{
    if (!ptr) return malloc(mCount, mSize);

    const usize requested = mSize * mCount;
    const usize realSize = alignUp8(requested);

    auto* pBlock = findBlockFromPtr(static_cast<u8*>(ptr));
    if (!pBlock)
    {
        errno = ENOBUFS;
        throw AllocException("pointer doesn't belong to this arena");
    }

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
        if (mCount <= oldCount) return ptr;

        auto* pRet = malloc(mCount, mSize);
        memcpy(pRet, ptr, oldCount * mSize);

        return pRet;
    }
}

inline void
Arena::free(void*) noexcept
{
    /* noop */
}

inline void
Arena::freeAll() noexcept
{
    auto* it = m_pBlocks;
    while (it)
    {
        auto* next = it->pNext;
        m_pBackAlloc->free(it);
        it = next;
    }
    m_pBlocks = nullptr;
}

inline void
Arena::reset() noexcept
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
Arena::shrinkToFirstBlock() noexcept
{
    auto* it = m_pBlocks;
    if (!it) return;

    while (it->pNext)
    {
#if defined ADT_DBG_MEMORY && !defined NDEBUG
        print::err("[Arena: {}, {}, {}]: shrinking {} sized block\n",
            print::stripSourcePath(m_loc.file_name()), m_loc.function_name(), m_loc.line(), it->size
        );
#endif
        auto* next = it->pNext;
        m_pBackAlloc->free(it);
        it = next;
    }
    m_pBlocks = it;
}

inline isize
Arena::nBytesOccupied() const noexcept
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

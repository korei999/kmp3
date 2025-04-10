#pragma once

#include "StdAllocator.hh"
#include "utils.hh"
#include "print.hh"

#include <cstdlib>
#include <cstring>

#if defined ADT_DBG_MEMORY
    #include <cstdio>
#endif

namespace adt
{

struct ArenaBlock
{
    ArenaBlock* pNext {};
    usize size {}; /* excluding sizeof(ArenaBlock) */
    usize nBytesOccupied {};
    u8* pLastAlloc {};
    usize lastAllocSize {};
    u8 pMem[];
};

/* fast region based allocator, only freeAll() free's memory, free() does nothing */
struct Arena : public IAllocator
{
    usize m_defaultCapacity {};
    IAllocator* m_pBackAlloc {};
    ArenaBlock* m_pBlocks {};

    /* */

    Arena() = default;

    Arena(usize capacity, IAllocator* pBackingAlloc = StdAllocator::inst()) noexcept(false)
        : m_defaultCapacity(align8(capacity)),
          m_pBackAlloc(pBackingAlloc),
          m_pBlocks(allocBlock(m_defaultCapacity)) {}

    /* */

    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false) override final;
    [[deprecated("Noop")]] virtual void free(void* ptr) noexcept override final; /* noop */
    virtual void freeAll() noexcept override final;
    void reset() noexcept;

    void shrinkToFirstBlock() noexcept;

    /* */

protected:
    [[nodiscard]] inline ArenaBlock* allocBlock(usize size);
    [[nodiscard]] inline ArenaBlock* prependBlock(usize size);
    [[nodiscard]] inline ArenaBlock* findFittingBlock(usize size);
    [[nodiscard]] inline ArenaBlock* findBlockFromPtr(u8* ptr);
};

inline ArenaBlock*
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

inline ArenaBlock*
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

inline ArenaBlock*
Arena::allocBlock(usize size)
{
    ADT_ASSERT(m_pBackAlloc, "uninitialized: m_pBackAlloc == nullptr");

    /* NOTE: m_pBackAlloc can throw here */
    ArenaBlock* pBlock = static_cast<ArenaBlock*>(m_pBackAlloc->zalloc(1, size + sizeof(ArenaBlock)));

#if defined ADT_DBG_MEMORY
    print::err("[Arena]: new block of size: {}\n", size);
#endif

    pBlock->size = size;
    pBlock->pLastAlloc = pBlock->pMem;

    return pBlock;
}

inline ArenaBlock*
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
    usize realSize = align8(mCount * mSize);
    auto* pBlock = findFittingBlock(realSize);

#if defined ADT_DBG_MEMORY
    if (m_defaultCapacity <= realSize)
        print::err("[Arena]: allocating more than defaultCapacity ({}, {})\n", m_defaultCapacity, realSize);
#endif

    if (!pBlock) pBlock = prependBlock(utils::max(m_defaultCapacity, realSize*2));

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
    memset(p, 0, align8(mCount * mSize));
    return p;
}

inline void*
Arena::realloc(void* ptr, usize oldCount, usize mCount, usize mSize)
{
    if (!ptr)
        return malloc(mCount, mSize);

    if (mCount <= oldCount)
        return ptr;

    usize requested = mSize * mCount;
    usize realSize = align8(requested);

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
#if defined ADT_DBG_MEMORY
        print::err("[Arena]: shrinking {} sized block\n", it->size);
#endif
        auto* next = it->pNext;
        m_pBackAlloc->free(it);
        it = next;
    }
    m_pBlocks = it;
}

} /* namespace adt */

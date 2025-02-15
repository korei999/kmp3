#pragma once

#include "OsAllocator.hh"

#include <cassert>
#include <cstdlib>
#include <cstring>

namespace adt
{

/* fixed byte size (chunk) per alloc. Calling realloc() is an error */
struct ChunkAllocatorNode
{
    ChunkAllocatorNode* next;
    u8 pNodeMem[];
};

struct ChunkAllocatorBlock
{
    ChunkAllocatorBlock* next = nullptr;
    ChunkAllocatorNode* head = nullptr;
    usize used = 0;
    u8 pMem[];
};

class ChunkAllocator : public IAllocator
{
    usize m_blockCap = 0; 
    usize m_chunkSize = 0;
    IAllocator* m_pBackAlloc {};
    ChunkAllocatorBlock* m_pBlocks = nullptr;

    /* */

public:
    ChunkAllocator() = default;
    ChunkAllocator(usize chunkSize, usize blockSize, IAllocator* pBackAlloc = OsAllocatorGet()) noexcept(false)
        : m_blockCap {align(blockSize, chunkSize + sizeof(ChunkAllocatorNode))},
          m_chunkSize {chunkSize + sizeof(ChunkAllocatorNode)},
          m_pBackAlloc(pBackAlloc),
          m_pBlocks {newBlock()} {}

    /* */

    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    ADT_WARN_IMPOSSIBLE_OPERATION virtual void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false) override final;
    void virtual free(void* ptr) noexcept override final;
    void virtual freeAll() noexcept override final;

    /* */

    template<typename T> ADT_WARN_IMPOSSIBLE_OPERATION constexpr T*
    reallocV(T* ptr, ssize oldCount, ssize newCount)
    { ADT_ASSERT_ALWAYS(false, "can't realloc"); return nullptr; };

private:
    [[nodiscard]] ChunkAllocatorBlock* newBlock();
};

inline ChunkAllocatorBlock*
ChunkAllocator::newBlock()
{
    usize total = m_blockCap + sizeof(ChunkAllocatorBlock);
    auto* r = (ChunkAllocatorBlock*)m_pBackAlloc->zalloc(1, total);

    r->head = (ChunkAllocatorNode*)r->pMem;

    ssize chunks = m_blockCap / m_chunkSize;

    auto* head = r->head;
    ChunkAllocatorNode* p = head;
    for (ssize i = 0; i < chunks - 1; ++i)
    {
        p->next = (ChunkAllocatorNode*)((u8*)p + m_chunkSize);
        p = p->next;
    }
    p->next = nullptr;

    return r;
}

inline void*
ChunkAllocator::malloc(usize, usize)
{
    ChunkAllocatorBlock* pBlock = m_pBlocks;
    ChunkAllocatorBlock* pPrev = nullptr;
    while (pBlock)
    {
        if (m_blockCap - pBlock->used >= m_chunkSize) break;
        else
        {
            pPrev = pBlock;
            pBlock = pBlock->next;
        }
    }

    if (!pBlock)
    {
        pPrev->next = newBlock();
        pBlock = pPrev->next;
    }

    auto* head = pBlock->head;
    pBlock->head = head->next;
    pBlock->used += m_chunkSize;

    return head->pNodeMem;
}

inline void*
ChunkAllocator::zalloc(usize, usize)
{
    auto* p = malloc(0, 0);
    memset(p, 0, m_chunkSize - sizeof(ChunkAllocatorNode));
    return p;
}

inline void*
ChunkAllocator::realloc(void*, usize, usize, usize)
{
    ADT_ASSERT_ALWAYS(false, "can't realloc()");
    return nullptr;
}

inline void
ChunkAllocator::free(void* p) noexcept
{
    if (!p) return;

    auto* node = (ChunkAllocatorNode*)((u8*)p - sizeof(ChunkAllocatorNode));

    auto* pBlock = m_pBlocks;
    while (pBlock)
    {
        if ((u8*)p > (u8*)pBlock->pMem && ((u8*)pBlock + m_blockCap) > (u8*)p)
            break;

        pBlock = pBlock->next;
    }

    assert(pBlock && "bad pointer?");
    
    node->next = pBlock->head;
    pBlock->head = node;
    pBlock->used -= m_chunkSize;
}

inline void
ChunkAllocator::freeAll() noexcept
{
    ChunkAllocatorBlock* p = m_pBlocks, * next = nullptr;
    while (p)
    {
        next = p->next;
        ::free(p);
        p = next;
    }
    m_pBlocks = nullptr;
}

} /* namespace adt */

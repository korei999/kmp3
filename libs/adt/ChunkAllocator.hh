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
    u64 used = 0;
    u8 pMem[];
};

class ChunkAllocator : public IAllocator
{
    u64 m_blockCap = 0; 
    u64 m_chunkSize = 0;
    IAllocator* m_pBackAlloc {};
    ChunkAllocatorBlock* m_pBlocks = nullptr;

    /* */

public:
    ChunkAllocator() = default;
    ChunkAllocator(u64 chunkSize, u64 blockSize, IAllocator* pBackAlloc = OsAllocatorGet())
        : m_blockCap {align(blockSize, chunkSize + sizeof(ChunkAllocatorNode))},
          m_chunkSize {chunkSize + sizeof(ChunkAllocatorNode)},
          m_pBackAlloc(pBackAlloc),
          m_pBlocks {newBlock()} {}

    /* */

    [[nodiscard]] virtual void* malloc(u64 mCount, u64 mSize) override final;
    [[nodiscard]] virtual void* zalloc(u64 mCount, u64 mSize) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, u64 mCount, u64 mSize) override final;
    void virtual free(void* ptr) override final;
    void virtual freeAll() override final;

    /* */

private:
    [[nodiscard]] ChunkAllocatorBlock* newBlock();
};

inline ChunkAllocatorBlock*
ChunkAllocator::newBlock()
{
    u64 total = m_blockCap + sizeof(ChunkAllocatorBlock);
    auto* r = (ChunkAllocatorBlock*)m_pBackAlloc->zalloc(1, total);
    assert(r != nullptr && "[ChunkAllocator]: calloc failed");
    r->head = (ChunkAllocatorNode*)r->pMem;

    u32 chunks = m_blockCap / m_chunkSize;

    auto* head = r->head;
    ChunkAllocatorNode* p = head;
    for (u64 i = 0; i < chunks - 1; i++)
    {
        p->next = (ChunkAllocatorNode*)((u8*)p + m_chunkSize);
        p = p->next;
    }
    p->next = nullptr;

    return r;
}

inline void*
ChunkAllocator::malloc([[maybe_unused]] u64 ignored0, [[maybe_unused]] u64 ignored1)
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
ChunkAllocator::zalloc([[maybe_unused]] u64 ignored0, [[maybe_unused]] u64 ignored1)
{
    auto* p = malloc(0, 0);
    memset(p, 0, m_chunkSize - sizeof(ChunkAllocatorNode));
    return p;
}

inline void*
ChunkAllocator::realloc(void*, u64, u64)
{
    assert(false && "ChunkAllocator can't realloc()");
    return nullptr;
}

inline void
ChunkAllocator::free(void* p)
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
ChunkAllocator::freeAll()
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

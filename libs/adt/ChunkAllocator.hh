#pragma once

#include "IAllocator.hh"

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

struct ChunkAllocator : IAllocator
{
    u64 m_blockCap = 0; 
    u64 m_chunkSize = 0;
    ChunkAllocatorBlock* m_pBlocks = nullptr;

    /* */

    ChunkAllocator() = default;
    ChunkAllocator(u64 chunkSize, u64 blockSize);

    /* */

    [[nodiscard]] virtual void* malloc(u64 mCount, u64 mSize) override final;
    [[nodiscard]] virtual void* zalloc(u64 mCount, u64 mSize) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, u64 mCount, u64 mSize) override final;
    void virtual free(void* ptr) override final;
    void virtual freeAll() override final;
};

inline ChunkAllocatorBlock*
_ChunkAllocatorNewBlock(ChunkAllocator* s)
{
    u64 total = s->m_blockCap + sizeof(ChunkAllocatorBlock);
    auto* r = (ChunkAllocatorBlock*)::calloc(1, total);
    assert(r != nullptr && "[ChunkAllocator]: calloc failed");
    r->head = (ChunkAllocatorNode*)r->pMem;

    u32 chunks = s->m_blockCap / s->m_chunkSize;

    auto* head = r->head;
    ChunkAllocatorNode* p = head;
    for (u64 i = 0; i < chunks - 1; i++)
    {
        p->next = (ChunkAllocatorNode*)((u8*)p + s->m_chunkSize);
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
        pPrev->next = _ChunkAllocatorNewBlock(this);
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
    auto* p = malloc(ignored0, ignored1);
    memset(p, 0, m_chunkSize);
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

inline
ChunkAllocator::ChunkAllocator(u64 chunkSize, u64 blockSize)
    : m_blockCap {align(blockSize, chunkSize + sizeof(ChunkAllocatorNode))},
      m_chunkSize {chunkSize + sizeof(ChunkAllocatorNode)},
      m_pBlocks {_ChunkAllocatorNewBlock(this)} {}

} /* namespace adt */

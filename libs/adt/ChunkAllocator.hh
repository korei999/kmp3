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

struct ChunkAllocator
{
    IAllocator super {};
    u64 blockCap = 0; 
    u64 chunkSize = 0;
    ChunkAllocatorBlock* pBlocks = nullptr;

    ChunkAllocator() = default;
    ChunkAllocator(u64 chunkSize, u64 blockSize);

    [[nodiscard]] void* alloc(u64 mCount, u64 mSize);
    [[nodiscard]] void* zalloc(u64 mCount, u64 mSize);
    [[nodiscard]] void* realloc(void* ptr, u64 mCount, u64 mSize);
    void free(void* ptr);
    void freeAll();
};

inline ChunkAllocatorBlock*
_ChunkAllocatorNewBlock(ChunkAllocator* s)
{
    u64 total = s->blockCap + sizeof(ChunkAllocatorBlock);
    auto* r = (ChunkAllocatorBlock*)::calloc(1, total);
    assert(r != nullptr && "[ChunkAllocator]: calloc failed");
    r->head = (ChunkAllocatorNode*)r->pMem;

    u32 chunks = s->blockCap / s->chunkSize;

    auto* head = r->head;
    ChunkAllocatorNode* p = head;
    for (u64 i = 0; i < chunks - 1; i++)
    {
        p->next = (ChunkAllocatorNode*)((u8*)p + s->chunkSize);
        p = p->next;
    }
    p->next = nullptr;

    return r;
}

inline void*
ChunkAllocator::alloc([[maybe_unused]] u64 ignored0, [[maybe_unused]] u64 ignored1)
{
    ChunkAllocatorBlock* pBlock = this->pBlocks;
    ChunkAllocatorBlock* pPrev = nullptr;
    while (pBlock)
    {
        if (this->blockCap - pBlock->used >= this->chunkSize) break;
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
    pBlock->used += this->chunkSize;

    return head->pNodeMem;
}

inline void*
ChunkAllocator::zalloc([[maybe_unused]] u64 ignored0, [[maybe_unused]] u64 ignored1)
{
    auto* p = this->alloc(ignored0, ignored1);
    memset(p, 0, this->chunkSize);
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

    auto* pBlock = this->pBlocks;
    while (pBlock)
    {
        if ((u8*)p > (u8*)pBlock->pMem && ((u8*)pBlock + this->blockCap) > (u8*)p)
            break;

        pBlock = pBlock->next;
    }

    assert(pBlock && "bad pointer?");
    
    node->next = pBlock->head;
    pBlock->head = node;
    pBlock->used -= this->chunkSize;
}

inline void
ChunkAllocator::freeAll()
{
    ChunkAllocatorBlock* p = this->pBlocks, * next = nullptr;
    while (p)
    {
        next = p->next;
        ::free(p);
        p = next;
    }
    this->pBlocks = nullptr;
}

inline const AllocatorVTable inl_ChunkAllocatorVTable = AllocatorVTableGenerate<ChunkAllocator>();

inline
ChunkAllocator::ChunkAllocator(u64 chunkSize, u64 blockSize)
    : super {&inl_ChunkAllocatorVTable},
      blockCap {align(blockSize, chunkSize + sizeof(ChunkAllocatorNode))},
      chunkSize {chunkSize + sizeof(ChunkAllocatorNode)},
      pBlocks {_ChunkAllocatorNewBlock(this)} {}

} /* namespace adt */

#pragma once

#include "Allocator.hh"

#include <cassert>
#include <cstdlib>
#include <cstring>

namespace adt
{

struct ChunkAllocator;

inline void* ChunkAlloc(ChunkAllocator* s, u64 ignored0, u64 ignored1);
inline void* ChunkZalloc(ChunkAllocator* s, u64 ignored0, u64 ignored1);
inline void ChunkFree(ChunkAllocator* s, void* p);
inline void ChunkFreeAll(ChunkAllocator* s);

inline void* alloc(ChunkAllocator* s, u64 mCount, u64 mSize) { return ChunkAlloc(s, mCount, mSize); }
inline void* zalloc(ChunkAllocator* s, u64 mCount, u64 mSize) { return ChunkZalloc(s, mCount, mSize); }
inline void free(ChunkAllocator* s, void* p) { ChunkFree(s, p); }
inline void freeAll(ChunkAllocator* s) { ChunkFreeAll(s); }

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

/* each alloc is the same size (good for linked data structures) */
struct ChunkAllocator
{
    Allocator base {};
    u64 blockCap = 0; 
    u64 chunkSize = 0;
    ChunkAllocatorBlock* pBlocks = nullptr;

    ChunkAllocator() = default;
    ChunkAllocator(u64 chunkSize, u64 blockSize);
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
ChunkAlloc(ChunkAllocator* s, [[maybe_unused]] u64 ignored0, [[maybe_unused]] u64 ignored1)
{
    ChunkAllocatorBlock* pBlock = s->pBlocks;
    ChunkAllocatorBlock* pPrev = nullptr;
    while (pBlock)
    {
        if (s->blockCap - pBlock->used >= s->chunkSize)
        {
            break;
        }
        else
        {
            pPrev = pBlock;
            pBlock = pBlock->next;
        }
    }

    if (!pBlock)
    {
        pPrev->next = _ChunkAllocatorNewBlock(s);
        pBlock = pPrev->next;
    }

    auto* head = pBlock->head;
    pBlock->head = head->next;
    pBlock->used += s->chunkSize;

    return head->pNodeMem;
}

inline void*
ChunkZalloc(ChunkAllocator* s, [[maybe_unused]] u64 ignored0, [[maybe_unused]] u64 ignored1)
{
    auto* p = ChunkAlloc(s, ignored0, ignored1);
    memset(p, 0, s->chunkSize);
    return p;
}

inline void*
_ChunkRealloc(
    [[maybe_unused]] ChunkAllocator* s,
    [[maybe_unused]] void* ___ignored,
    [[maybe_unused]] u64 _ignored,
    [[maybe_unused]] u64 __ignored
)
{
    assert(false && "ChunkAllocator can't realloc");
    return nullptr;
}

inline void
ChunkFree(ChunkAllocator* s, void* p)
{
    if (!p) return;

    auto* node = (ChunkAllocatorNode*)((u8*)p - sizeof(ChunkAllocatorNode));

    auto* pBlock = s->pBlocks;
    while (pBlock)
    {
        if ((u8*)p > (u8*)pBlock->pMem && ((u8*)pBlock + s->blockCap) > (u8*)p)
            break;

        pBlock = pBlock->next;
    }

    assert(pBlock && "bad pointer?");
    
    node->next = pBlock->head;
    pBlock->head = node;
    pBlock->used -= s->chunkSize;
}

inline void
ChunkFreeAll(ChunkAllocator* s)
{
    ChunkAllocatorBlock* p = s->pBlocks, * next = nullptr;
    while (p)
    {
        next = p->next;
        ::free(p);
        p = next;
    }
    s->pBlocks = nullptr;
}

inline const AllocatorInterface inl_chunkAllocatorVTable {
    .alloc = decltype(AllocatorInterface::alloc)(ChunkAlloc),
    .zalloc = decltype(AllocatorInterface::zalloc)(ChunkZalloc),
    .realloc = decltype(AllocatorInterface::realloc)(_ChunkRealloc),
    .free = decltype(AllocatorInterface::free)(ChunkFree),
    .freeAll = decltype(AllocatorInterface::freeAll)(ChunkFreeAll),
};

inline
ChunkAllocator::ChunkAllocator(u64 chunkSize, u64 blockSize)
    : base {&inl_chunkAllocatorVTable},
      blockCap {align(blockSize, chunkSize + sizeof(ChunkAllocatorNode))},
      chunkSize {chunkSize + sizeof(ChunkAllocatorNode)},
      pBlocks {_ChunkAllocatorNewBlock(this)} {}

} /* namespace adt */

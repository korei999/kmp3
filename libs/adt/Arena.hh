#pragma once

#include "Allocator.hh"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstddef>

#ifndef NDEBUG
    #include <cstdio>
#endif

#define ADT_ARENA_FIRST(A) ((A)->pBlocksHead)
#define ADT_ARENA_NEXT(AB) ((AB)->pNext)
#define ADT_ARENA_FOREACH(A, IT) for (ArenaBlock* IT = ADT_ARENA_FIRST(A); IT; IT = ADT_ARENA_NEXT(IT))
#define ADT_ARENA_FOREACH_SAFE(A, IT, TMP) for (ArenaBlock* IT = ADT_ARENA_FIRST(A), * TMP = nullptr; IT && ((TMP) = ADT_ARENA_NEXT(IT), true); (IT) = (TMP))
#define ADT_ARENA_GET_NODE_FROM_BLOCK(PB) ((ArenaNode*)((u8*)(PB) + offsetof(ArenaBlock, pData)))
#define ADT_ARENA_GET_NODE_FROM_DATA(PD) ((ArenaNode*)((u8*)(PD) - offsetof(ArenaNode, pData)))

namespace adt
{

struct ArenaNode;

struct ArenaBlock
{
    ArenaBlock* pNext = nullptr;
    u64 size = 0;
    ArenaNode* pLast = nullptr;
    u8 pData[]; /* flexible array member */
};

struct ArenaNode
{
    ArenaNode* pNext = nullptr;
    u8 pData[];
};

struct Arena
{
    Allocator base {};
    ArenaBlock* pBlocksHead = nullptr;
    ArenaBlock* pLastBlockAllocation = nullptr;

    Arena() = default;
    Arena(u32 blockCap);
};

inline void* ArenaAlloc(Arena* s, u64 mCount, u64 mSize);
inline void* ArenaRealloc(Arena* s, void* p, u64 mCount, u64 mSize);
inline void _ArenaFree(Arena* s, void* p);
inline void ArenaReset(Arena* s);
inline void ArenaFreeAll(Arena* s);

inline void* alloc(Arena* s, u64 mCount, u64 mSize) { return ArenaAlloc(s, mCount, mSize); }
inline void* realloc(Arena* s, void* p, u64 mCount, u64 mSize) { return ArenaRealloc(s, p, mCount, mSize); }
inline void free(Arena* s, void* p) { _ArenaFree(s, p); }
inline void freeAll(Arena* s) { ArenaFreeAll(s); }

inline ArenaBlock* _ArenaAllocatorNewBlock(Arena* s, u64 size);

inline ArenaBlock*
_ArenaAllocatorNewBlock(Arena* s, u64 size)
{
    ArenaBlock** ppLastBlock = &s->pBlocksHead;
    while (*ppLastBlock) ppLastBlock = &((*ppLastBlock)->pNext);

    u64 addedSize = size + sizeof(ArenaBlock);
    *ppLastBlock = (ArenaBlock*)(calloc(1, addedSize));
    assert(*ppLastBlock != nullptr && "[Arena]: calloc failed");

    auto* pBlock = (ArenaBlock*)*ppLastBlock;
    pBlock->size = size;
    auto* pNode = ADT_ARENA_GET_NODE_FROM_BLOCK(*ppLastBlock);
    pNode->pNext = pNode; /* don't bump the very first node on `alloc()` */
    pBlock->pLast = pNode;
    s->pLastBlockAllocation = *ppLastBlock;

    return *ppLastBlock;
}

inline void*
ArenaAlloc(Arena* s, u64 mCount, u64 mSize)
{
    u64 requested = mCount * mSize;
    u64 aligned = align8(requested) + sizeof(ArenaNode);

    ArenaBlock* pFreeBlock = s->pBlocksHead;

    while (aligned >= pFreeBlock->size)
    {
#ifndef NDEBUG
        fprintf(stderr,
            "[ARENA]: requested size > than one block\n"
            "\taligned: %zu, blockSize: %zu, requested: %zu\n",
            aligned, pFreeBlock->size, requested
        );
#endif

        pFreeBlock = pFreeBlock->pNext;
        if (!pFreeBlock) pFreeBlock =  _ArenaAllocatorNewBlock(s, aligned * 2); /* NOTE: trying to double too big of an array situation */
    }

repeat:
    /* skip pNext */
    ArenaNode* pNode = ADT_ARENA_GET_NODE_FROM_BLOCK(pFreeBlock);
    ArenaNode* pNextNode = pFreeBlock->pLast->pNext;
    u64 nextAligned = ((u8*)pNextNode + aligned) - (u8*)pNode;

    /* heap overflow */
    if (nextAligned >= pFreeBlock->size)
    {
        pFreeBlock = pFreeBlock->pNext;
        if (!pFreeBlock) pFreeBlock = _ArenaAllocatorNewBlock(s, nextAligned);
        goto repeat;
    }

    auto* nextAddr = (ArenaNode*)((u8*)pNextNode + aligned);
    pNextNode->pNext = nextAddr;
    pFreeBlock->pLast = pNextNode;

    return &pNextNode->pData;
}

inline void*
ArenaRealloc(Arena* s, void* p, u64 mCount, u64 mSize)
{
    ArenaNode* pNode = ADT_ARENA_GET_NODE_FROM_DATA(p);
    ArenaBlock* pBlock = nullptr;

    /* figure out which block this node belongs to */
    ADT_ARENA_FOREACH(s, pB)
    {
        if ((u8*)p > (u8*)pB && ((u8*)pB + pB->size) > (u8*)p)
        {
            pBlock = pB;
            break;
        }
    }

    assert(pBlock != nullptr && "block not found, bad pointer");

    auto aligned = align8(mCount * mSize);
    u64 nextAligned = ((u8*)pNode + aligned) - (u8*)ADT_ARENA_GET_NODE_FROM_BLOCK(pBlock);

    if (pNode == pBlock->pLast && nextAligned < pBlock->size)
    {
        /* NOTE: + sizeof(ArenaNode) is necessary */
        auto* nextAddr = (ArenaNode*)((u8*)pNode + aligned + sizeof(ArenaNode));

        pNode->pNext = nextAddr;

        return p;
    }
    else
    {
        void* pR = ArenaAlloc(s, mCount, mSize);
        /* BUG?: sanitizer sees heap-overflow without - sizeof(ArenaNode) */
        memcpy(pR, p, ((u8*)pNode->pNext - (u8*)pNode) - sizeof(ArenaNode));

        return pR;
    }
}

inline void
_ArenaFree([[maybe_unused]] Arena* s, [[maybe_unused]] void* p)
{
    // TODO: it's possible to free the last allocation i guess
}

inline void
ArenaReset(Arena* s)
{
    ADT_ARENA_FOREACH(s, pB)
    {
        ArenaNode* pNode = ADT_ARENA_GET_NODE_FROM_BLOCK(pB);
        pB->pLast = pNode;
        pNode->pNext = pNode;
    }

    auto first = ADT_ARENA_FIRST(s);
    s->pLastBlockAllocation = first;
}

inline void
ArenaFreeAll(Arena* s)
{
    ADT_ARENA_FOREACH_SAFE(s, pB, tmp)
        ::free(pB);
}

inline const AllocatorInterface inl_ArenaAllocatorVTable {
    .alloc = decltype(AllocatorInterface::alloc)(ArenaAlloc),
    .realloc = decltype(AllocatorInterface::realloc)(ArenaRealloc),
    .free = decltype(AllocatorInterface::free)(_ArenaFree),
    .freeAll = decltype(AllocatorInterface::freeAll)(ArenaFreeAll),
};

inline 
Arena::Arena(u32 blockCap)
    : base {&inl_ArenaAllocatorVTable}
{
    _ArenaAllocatorNewBlock(this, align8(blockCap + sizeof(ArenaNode)));
}

} /* namespace adt */

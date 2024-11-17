#pragma once

#include "Allocator.hh"
#include "utils.hh"

#include <cassert>
#include <cstdlib>

namespace adt
{

struct BuddyBlock
{
    BuddyBlock* pNext {};
    u64 size {};
    u64 nBytesOccupied {};
    u8 pMem[];
};

struct BuddyNode
{
    /* NOTE: saving isFree boolean in the leftmost bit and the rest is the size */
    u64 sizeAndIsFree {};
    u8 pMem[];
};

/* FIXME: terribly slow buddy allocator */
struct Buddy
{
    Allocator base {};
    BuddyBlock* pBlocks {};
    u64 blockSize {};

    Buddy() = default;
    Buddy(u64 blockSize);
};

constexpr u64
BuddyNodeSize(const BuddyNode* s)
{
    return s->sizeAndIsFree & ~(1UL << 63);
}

constexpr bool
BuddyNodeIsFree(const BuddyNode* s)
{
    return s->sizeAndIsFree & (1UL << 63);
}

constexpr void
BuddyNodeSetFree(BuddyNode* s, bool bFree)
{
    bFree ? s->sizeAndIsFree |= (1UL << 63) : s->sizeAndIsFree &= ~(1UL << 63);
}

constexpr void
BuddyNodeSetSizeSetFree(BuddyNode* s, u64 size, bool bFree)
{
    s->sizeAndIsFree = size;
    BuddyNodeSetFree(s, bFree);
}

constexpr BuddyNode*
BuddyNodeFromBlock(const BuddyBlock* pBlock)
{
    return (BuddyNode*)pBlock->pMem;
}

constexpr BuddyNode*
BuddyNodeNext(const BuddyNode* s)
{
    return (BuddyNode*)((u8*)s + BuddyNodeSize(s));
}

constexpr BuddyNode*
BuddyNodeFromPtr(void* p)
{
    return (BuddyNode*)((u8*)p - sizeof(BuddyNode));
}

inline BuddyBlock*
BuddyBlockNew(u64 size)
{
    assert(size >= 128 && "[Buddy]: too small");

    u64 aligned = nextPowerOf2(size);
    auto* pNewBlock = (BuddyBlock*)::calloc(1, aligned + sizeof(BuddyBlock));
    pNewBlock->size = aligned + sizeof(BuddyBlock);

    BuddyNode* pNode = BuddyNodeFromBlock(pNewBlock);
    BuddyNodeSetSizeSetFree(pNode, aligned, true);

    return pNewBlock;
};

inline BuddyBlock*
BuddyBlockPrepend(Buddy* s, u64 size)
{
    assert(s->pBlocks && "[Buddy]: ininitialized prepend");
    auto* pNew = BuddyBlockNew(size);
    pNew->pNext = s->pBlocks;
    s->pBlocks = pNew;

    return pNew;
}

inline BuddyNode*
BuddyNodeSplit(BuddyNode* s, const u64 size)
{
    while (BuddyNodeSize(s)/2 >= size + sizeof(BuddyNode))
    {
        u64 half = BuddyNodeSize(s) / 2;
        BuddyNodeSetSizeSetFree(s, half, true);
        s = BuddyNodeNext(s);
        BuddyNodeSetSizeSetFree(s, half, true);
    }

    assert(BuddyNodeIsFree(s) && "mustn't split nonfree node");
    return s;
}

inline void*
BuddyAlloc(Buddy* s, u64 nMembers, u64 memeberSize)
{
    assert(s->pBlocks && "[Buddy]: ininitialized alloc");

    u64 requested = nMembers * memeberSize;

    auto* pBlock = s->pBlocks;
    while (pBlock && pBlock->size - sizeof(BuddyBlock) < requested)
        pBlock = pBlock->pNext;

    bool bNoBlock = !pBlock;
    bool bBlockIsOutOfSpace = pBlock && (pBlock->size - sizeof(BuddyBlock)) < (pBlock->nBytesOccupied + nextPowerOf2(requested + sizeof(BuddyNode)));
    if (bNoBlock || bBlockIsOutOfSpace)
    {
again:
        u64 adjSize = utils::max(s->blockSize, requested*2);
        pBlock = BuddyBlockPrepend(s, adjSize);
    }

    /* find free node that fits requested + sizeof(BuddyNode) */
    u8* pTail = (u8*)pBlock + pBlock->size;
    auto* pNode = BuddyNodeFromBlock(pBlock);

    while ((u8*)pNode < pTail)
    {
        if (!BuddyNodeIsFree(pNode)) pNode = BuddyNodeNext(pNode);
        else
        {
            if (BuddyNodeSize(pNode) >= requested + sizeof(BuddyNode)) break;
            else pNode = BuddyNodeNext(pNode);
        }
    }

    /* if all the nodes are too small */
    if ((u8*)pNode == pTail)
        goto again;

    pNode = BuddyNodeSplit(pNode, requested);

    BuddyNodeSetFree(pNode, false);
    pBlock->nBytesOccupied += BuddyNodeSize(pNode);;

    return pNode->pMem;
}

inline BuddyBlock*
BuddyBlockOfNode(Buddy* s, BuddyNode* pNode)
{
    assert(s->pBlocks && "[Buddy]: ininitialized");
    auto* pBlock = s->pBlocks;
    while (pBlock)
    {
        auto* pTail = ((u8*)pBlock + pBlock->size);
        if ((u8*)pNode > (u8*)pBlock && (u8*)pNode < pTail) break;
        pBlock = pBlock->pNext;
    }

    assert(pBlock && "[Buddy]: couldn't find block");
    return pBlock;
}

inline void
BuddyCoalescence(const BuddyBlock* pBlock, const BuddyNode* pNode)
{
    auto* pTail = ((u8*)pBlock + pBlock->size);

    auto* pPrev = pNode;
    auto* pNext = BuddyNodeNext(pNode);

    while (pPrev && BuddyNodeIsFree(pPrev) && pNext && BuddyNodeIsFree(pNext))
    {
        pPrev = pNext;
        pNext = BuddyNodeNext(pNext);
    }
}

inline void
BuddyFree(Buddy* s, void* p)
{
    if (!p) return;

    auto* pNode = BuddyNodeFromPtr(p);
    auto* pBlock = BuddyBlockOfNode(s, pNode);
    pBlock->nBytesOccupied -= BuddyNodeSize(pNode);
    BuddyNodeSetFree(pNode, true);

    /*BuddyCoalescence(pBlock, pNode);*/
}

inline void*
BuddyRealloc(Buddy* s, void* p, u64 nMembers, u64 mSize)
{
    auto* pNode = BuddyNodeFromPtr(p);
    void* ret = BuddyAlloc(s, nMembers, mSize);
    memcpy(ret, p, BuddyNodeSize(pNode) - sizeof(BuddyNode));
    BuddyFree(s, p);

    return ret;
}

inline void
BuddyFreeAll(Buddy* s)
{
    auto* it = s->pBlocks;
    while (it)
    {
        auto* next = it->pNext;
        ::free(it);
        it = next;
    }
}

inline const AllocatorInterface inl_BuddyAllocatorVTable {
    .alloc = decltype(AllocatorInterface::alloc)(BuddyAlloc),
    .realloc = decltype(AllocatorInterface::realloc)(BuddyRealloc),
    .free = decltype(AllocatorInterface::free)(BuddyFree),
    .freeAll = decltype(AllocatorInterface::freeAll)(BuddyFreeAll),
};

inline Buddy::Buddy(u64 _blockSize)
    : base(&inl_BuddyAllocatorVTable),
      pBlocks(BuddyBlockNew(_blockSize)),
      blockSize(nextPowerOf2(_blockSize)) {}

inline void* alloc(Buddy* s, u64 mCount, u64 mSize) { return BuddyAlloc(s, mCount, mSize); }
inline void* realloc(Buddy* s, void* p, u64 mCount, u64 mSize) { return BuddyRealloc(s, p, mCount, mSize); }
inline void free(Buddy* s, void* p) { BuddyFree(s, p); }
inline void freeAll(Buddy* s) { BuddyFreeAll(s); }

} /* namespace adt */

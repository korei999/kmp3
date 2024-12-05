#pragma once

#include "IAllocator.hh"
#include "utils.hh"

#include <cassert>
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
    u64 size {}; /* excluding sizeof(ArenaBlock) */
    u64 nBytesOccupied {};
    u8* pLastAlloc {};
    u64 lastAllocSize {};
    u8 pMem[];
};

/* fast region based allocator, only freeAll() free's memory, free() does nothing */
struct Arena
{
    IAllocator super {};
    u64 defaultCapacity {};
    ArenaBlock* pBlocks {};

    Arena() = default;
    Arena(u64 capacity);
};

[[nodiscard]] inline void* ArenaAlloc(Arena* s, u64 mCount, u64 mSize);
[[nodiscard]] inline void* ArenaZalloc(Arena* s, u64 mCount, u64 mSize);
[[nodiscard]] inline void* ArenaRealloc(Arena* s, void* ptr, u64 mCount, u64 mSize);
inline void ArenaFree(Arena* s, void* ptr);
inline void ArenaFreeAll(Arena* s);
inline void ArenaReset(Arena* s);

[[nodiscard]] inline void* alloc(Arena* s, u64 mCount, u64 mSize) { return ArenaAlloc(s, mCount, mSize); }
[[nodiscard]] inline void* zalloc(Arena* s, u64 mCount, u64 mSize) { return ArenaZalloc(s, mCount, mSize); }
[[nodiscard]] inline void* realloc(Arena* s, void* ptr, u64 mCount, u64 mSize) { return ArenaRealloc(s, ptr, mCount, mSize); }
inline void free(Arena* s, void* ptr) { return ArenaFree(s, ptr); }
inline void freeAll(Arena* s) { return ArenaFreeAll(s); }

[[nodiscard]] inline ArenaBlock*
_ArenaFindBlockFromPtr(Arena* s, u8* ptr)
{
    auto* it = s->pBlocks;
    while (it)
    {
        if (ptr >= it->pMem && ptr < &it->pMem[it->size])
            return it;

        it = it->pNext;
    }

    return nullptr;
}

[[nodiscard]] inline ArenaBlock*
_ArenaFindFittingBlock(Arena* s, u64 size)
{
    auto* it = s->pBlocks;
    while (it)
    {
        if (it->size - it->nBytesOccupied > size)
            return it;

        it = it->pNext;
    }

    return nullptr;
}

[[nodiscard]] inline ArenaBlock*
_ArenaAllocBlock(u64 size)
{
    ArenaBlock* pBlock = (ArenaBlock*)::calloc(1, size + sizeof(ArenaBlock));
    pBlock->size = size;
    pBlock->pLastAlloc = pBlock->pMem;

    return pBlock;
}

[[nodiscard]] inline ArenaBlock*
_ArenaPrependBlock(Arena* s, u64 size)
{
    auto* pNew = _ArenaAllocBlock(size);
    pNew->pNext = s->pBlocks;
    s->pBlocks = pNew;

    return pNew;
}

inline void*
ArenaAlloc(Arena* s, u64 mCount, u64 mSize)
{
    u64 realSize = align8(mCount * mSize);
    auto* pBlock = _ArenaFindFittingBlock(s, realSize);

#if defined ADT_DBG_MEMORY
    if (s->defaultCapacity <= realSize)
        fprintf(stderr, "[Arena]: allocating more than defaultCapacity (%llu, %llu)\n", s->defaultCapacity, realSize);
#endif

    if (!pBlock) pBlock = _ArenaPrependBlock(s, utils::max(s->defaultCapacity, realSize*2));

    auto* pRet = pBlock->pLastAlloc + pBlock->lastAllocSize;

    pBlock->pLastAlloc = pRet;
    pBlock->nBytesOccupied += realSize;
    pBlock->lastAllocSize = realSize;

    return pRet;
}

inline void*
ArenaZalloc(Arena* s, u64 mCount, u64 mSize)
{
    auto* p = ArenaAlloc(s, mCount, mSize);
    memset(p, 0, mCount * mSize);
    return p;
}

inline void*
ArenaRealloc(Arena* s, void* ptr, u64 mCount, u64 mSize)
{
    if (!ptr) return ArenaAlloc(s, mCount, mSize);

    u64 requested = mSize * mCount;
    u64 realSize = align8(requested);
    auto* pBlock = _ArenaFindBlockFromPtr(s, (u8*)ptr);

    assert(pBlock && "[Arena]: pointer doesn't belong to this arena");

    if (ptr == pBlock->pLastAlloc &&
        pBlock->pLastAlloc + realSize < pBlock->pMem + pBlock->size) /* bump case */
    {
        if (pBlock->lastAllocSize >= requested) return ptr;

        pBlock->nBytesOccupied -= pBlock->lastAllocSize;
        pBlock->nBytesOccupied += realSize;
        pBlock->lastAllocSize = realSize;

        return ptr;
    }
    else
    {
        auto* pRet = ArenaAlloc(s, mCount, mSize);
        u64 nBytesUntilEndOfBlock = &pBlock->pMem[pBlock->size] - (u8*)ptr;
        u64 nBytesToCopy = utils::min(requested, nBytesUntilEndOfBlock); /* out of range memcpy */
        nBytesToCopy = utils::min(nBytesToCopy, u64((u8*)pRet - (u8*)ptr)); /* overlap memcpy */
        memcpy(pRet, ptr, nBytesToCopy);

        return pRet;
    }
}

inline void
ArenaFree([[maybe_unused]] Arena* s, [[maybe_unused]] void* ptr)
{
    //
}

inline void
ArenaFreeAll(Arena* s)
{
    auto* it = s->pBlocks;
    while (it)
    {
        auto* next = it->pNext;
        ::free(it);
        it = next;
    }
    s->pBlocks = nullptr;
}

inline void
ArenaReset(Arena* s)
{
    auto* it = s->pBlocks;
    while (it)
    {
        it->nBytesOccupied = 0;
        it->lastAllocSize = 0;
        it->pLastAlloc = it->pMem;

        it = it->pNext;
    }
}

inline const AllocatorVTable inl_ArenaVTable {
    .alloc = decltype(AllocatorVTable::alloc)(ArenaAlloc),
    .zalloc = decltype(AllocatorVTable::zalloc)(ArenaZalloc),
    .realloc = decltype(AllocatorVTable::realloc)(ArenaRealloc),
    .free = decltype(AllocatorVTable::free)(ArenaFree),
    .freeAll = decltype(AllocatorVTable::freeAll)(ArenaFreeAll),
};

inline Arena::Arena(u64 capacity)
    : super(&inl_ArenaVTable),
      defaultCapacity(align8(capacity)),
      pBlocks(_ArenaAllocBlock(this->defaultCapacity)) {}

} /* namespace adt */

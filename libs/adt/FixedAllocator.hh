#pragma once

#include "Allocator.hh"

#include <cassert>
#include <cstring>

namespace adt
{

struct FixedAllocator
{
    Allocator base {};
    u8* pMemBuffer = nullptr;
    u64 size = 0;
    u64 cap = 0;
    void* pLastAlloc = nullptr;

    constexpr FixedAllocator() = default;
    constexpr FixedAllocator(void* pMemory, u64 capacity);
};

constexpr void* FixedAllocatorAlloc(FixedAllocator* s, u64 mCount, u64 mSize);
constexpr void* FixedAllocatorRealloc(FixedAllocator* s, void* p, u64 mCount, u64 mSize);
constexpr void FixedAllocatorFree(FixedAllocator* s, void* p);
constexpr void FixedAllocatorFreeAll(FixedAllocator* s);
constexpr void FixedAllocatorReset(FixedAllocator* s);

inline void* alloc(FixedAllocator* s, u64 mCount, u64 mSize) { return FixedAllocatorAlloc(s, mCount, mSize); }
inline void* realloc(FixedAllocator* s, void* p, u64 mCount, u64 mSize) { return FixedAllocatorRealloc(s, p, mCount, mSize); }
inline void free(FixedAllocator* s, void* p) { return FixedAllocatorFree(s, p); }
inline void freeAll(FixedAllocator* s) { return FixedAllocatorFreeAll(s); }

constexpr void*
FixedAllocatorAlloc(FixedAllocator* s, u64 mCount, u64 mSize)
{
    u64 aligned = align8(mCount * mSize);
    void* ret = &s->pMemBuffer[s->size];
    s->size += aligned;
    s->pLastAlloc = ret;

    assert(s->size < s->cap && "Out of memory");

    return ret;
}

constexpr void*
FixedAllocatorRealloc(FixedAllocator* s, void* p, u64 mCount, u64 mSize)
{
    void* ret = nullptr;
    u64 aligned = align8(mCount * mSize);

    if (p == s->pLastAlloc)
    {
        s->size -= (u8*)&s->pMemBuffer[s->size] - (u8*)p;
        s->size += aligned;

        return p;
    }
    else
    {
        ret = &s->pMemBuffer[s->size];
        s->pLastAlloc = ret;
        memcpy(ret, p, aligned);
        s->size += aligned;
    }

    return ret;
}

constexpr void
FixedAllocatorFree([[maybe_unused]] FixedAllocator* s, [[maybe_unused]] void* p)
{
    //
}

constexpr void
FixedAllocatorFreeAll([[maybe_unused]] FixedAllocator* s)
{
    //
}

constexpr void
FixedAllocatorReset(FixedAllocator* s)
{
    s->size = 0;
}

inline const AllocatorInterface __FixedAllocatorVTable {
    .alloc = decltype(AllocatorInterface::alloc)(FixedAllocatorAlloc),
    .realloc = decltype(AllocatorInterface::realloc)(FixedAllocatorRealloc),
    .free = decltype(AllocatorInterface::free)(FixedAllocatorFree),
    .freeAll = decltype(AllocatorInterface::freeAll)(FixedAllocatorFreeAll),
};

constexpr FixedAllocator::FixedAllocator(void* pMemory, u64 capacity)
    : base{.pVTable = &__FixedAllocatorVTable}, pMemBuffer((u8*)pMemory), cap(capacity) {}

} /* namespace adt */

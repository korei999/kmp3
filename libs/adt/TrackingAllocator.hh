#pragma once

#include "Allocator.hh"
#include "Map.hh"
#include "OsAllocator.hh"

#include <stdlib.h>

namespace adt
{

/* simple gen-purpose calloc/realloc/free/freeAll, while tracking allocations */
struct TrackingAllocator
{
    Allocator base;
    MapBase<void*> mAllocations;

    TrackingAllocator() = default;
    TrackingAllocator(u64 pre);
};

inline void*
TrackingAlloc(TrackingAllocator* s, u64 mCount, u64 mSize)
{
    void* r = ::malloc(mCount * mSize);
    MapInsert(&s->mAllocations, &s->base, r);
    return r;
}

inline void*
TrackingZalloc(TrackingAllocator* s, u64 mCount, u64 mSize)
{
    void* r = ::calloc(mCount, mSize);
    MapInsert(&s->mAllocations, &s->base, r);
    return r;
}

inline void*
TrackingRealloc(TrackingAllocator* s, void* p, u64 mCount, u64 mSize)
{
    void* r = ::realloc(p, mCount * mSize);

    if (p != r)
    {
        MapRemove(&s->mAllocations, p);
        MapInsert(&s->mAllocations, &s->base, r);
    }

    return r;
}

inline void
TrackingFree(TrackingAllocator* s, void* p)
{
    MapRemove(&s->mAllocations, p);
    ::free(p);
}

inline void
TrackingFreeAll(TrackingAllocator* s)
{
    for (auto& b : s->mAllocations)
        ::free(b);

    MapDestroy(&s->mAllocations, &s->base);
}

inline const AllocatorInterface inl_TrackingAllocatorVTable {
    .alloc = decltype(AllocatorInterface::alloc)(TrackingAlloc),
    .zalloc = decltype(AllocatorInterface::zalloc)(TrackingZalloc),
    .realloc = decltype(AllocatorInterface::realloc)(TrackingRealloc),
    .free = decltype(AllocatorInterface::free)(TrackingFree),
    .freeAll = decltype(AllocatorInterface::freeAll)(TrackingFreeAll),
};

inline
TrackingAllocator::TrackingAllocator(u64 pre)
    : base {&inl_TrackingAllocatorVTable}, mAllocations(&inl_OsAllocator.base, pre * 2) {}

[[nodiscard]] inline void* alloc(TrackingAllocator* s, u64 mCount, u64 mSize) { return TrackingAlloc(s, mCount, mSize); }
[[nodiscard]] inline void* zalloc(TrackingAllocator* s, u64 mCount, u64 mSize) { return TrackingZalloc(s, mCount, mSize); }
[[nodiscard]] inline void* realloc(TrackingAllocator* s, void* p, u64 mCount, u64 mSize) { return TrackingRealloc(s, p, mCount, mSize); }
inline void free(TrackingAllocator* s, void* p) { TrackingFree(s, p); }
inline void freeAll(TrackingAllocator* s) { TrackingFreeAll(s); }

} /* namespace adt */

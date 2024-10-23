#pragma once

#include "Allocator.hh"

#include <cassert>
#include <cstdlib>

namespace adt
{

struct OsAllocator;

inline void* OsAlloc(OsAllocator* s, u64 mCount, u64 mSize);
inline void* OsRealloc(OsAllocator* s, void* p, u64 mCount, u64 mSize);
inline void OsFree(OsAllocator* s, void* p);
inline void __OsFreeAll(OsAllocator* s);

inline void* alloc(OsAllocator* s, u64 mCount, u64 mSize) { return OsAlloc(s, mCount, mSize); }
inline void* realloc(OsAllocator* s, void* p, u64 mCount, u64 mSize) { return OsRealloc(s, p, mCount, mSize); }
inline void free(OsAllocator* s, void* p) { OsFree(s, p); }
inline void freeAll(OsAllocator* s) { __OsFreeAll(s); }

inline const AllocatorInterface _inl_OsAllocatorVTable {
    .alloc = (decltype(AllocatorInterface::alloc))OsAlloc,
    .realloc = (decltype(AllocatorInterface::realloc))OsRealloc,
    .free = (decltype(AllocatorInterface::free))OsFree,
    .freeAll = (decltype(AllocatorInterface::freeAll))__OsFreeAll,
};

struct OsAllocator
{
    Allocator base {};

    constexpr OsAllocator([[maybe_unused]] u32 _ingnored = 0) : base {&_inl_OsAllocatorVTable} {}
};

inline void*
OsAlloc([[maybe_unused]] OsAllocator* s, u64 mCount, u64 mSize)
{
    return ::calloc(mCount, mSize);
}

inline void*
OsRealloc([[maybe_unused]] OsAllocator* s, void* p, u64 mCount, u64 mSize)
{
    return ::realloc(p, mCount * mSize);
}

inline void
OsFree([[maybe_unused]] OsAllocator* s, void* p)
{
    ::free(p);
}

inline void
__OsFreeAll([[maybe_unused]] OsAllocator* s)
{
    assert(false && "OsAllocator: no 'freeAll()' method");
}

inline OsAllocator inl_OsAllocator {};

} /* namespace adt */

#pragma once

#include "Allocator.hh"

#include <cassert>
#include <cstdlib>

namespace adt
{

struct OsAllocator;

inline void* OsAlloc(OsAllocator* s, u64 mCount, u64 mSize);
inline void* OsZalloc(OsAllocator* s, u64 mCount, u64 mSize);
inline void* OsRealloc(OsAllocator* s, void* p, u64 mCount, u64 mSize);
inline void OsFree(OsAllocator* s, void* p);
inline void _OsFreeAll(OsAllocator* s);

inline void* alloc(OsAllocator* s, u64 mCount, u64 mSize) { return OsAlloc(s, mCount, mSize); }
inline void* zalloc(OsAllocator* s, u64 mCount, u64 mSize) { return OsZalloc(s, mCount, mSize); }
inline void* realloc(OsAllocator* s, void* p, u64 mCount, u64 mSize) { return OsRealloc(s, p, mCount, mSize); }
inline void free(OsAllocator* s, void* p) { OsFree(s, p); }
inline void freeAll(OsAllocator* s) { _OsFreeAll(s); }

inline const AllocatorInterface inl_OsAllocatorVTable {
    .alloc = (decltype(AllocatorInterface::alloc))OsAlloc,
    .zalloc = (decltype(AllocatorInterface::zalloc))OsZalloc,
    .realloc = (decltype(AllocatorInterface::realloc))OsRealloc,
    .free = (decltype(AllocatorInterface::free))OsFree,
    .freeAll = (decltype(AllocatorInterface::freeAll))_OsFreeAll,
};

struct OsAllocator
{
    Allocator super {};

    constexpr OsAllocator([[maybe_unused]] u32 _ingnored = 0) : super(&inl_OsAllocatorVTable) {}
};

inline void*
OsAlloc([[maybe_unused]] OsAllocator* s, u64 mCount, u64 mSize)
{
    auto* r = ::malloc(mCount * mSize);
    assert(r != nullptr && "[OsAllocator]: calloc failed");
    return r;
}

inline void*
OsZalloc([[maybe_unused]] OsAllocator* s, u64 mCount, u64 mSize)
{
    auto* r = ::calloc(mCount, mSize);
    assert(r != nullptr && "[OsAllocator]: calloc failed");
    return r;
}

inline void*
OsRealloc([[maybe_unused]] OsAllocator* s, void* p, u64 mCount, u64 mSize)
{
    auto* r = ::realloc(p, mCount * mSize);
    assert(r != nullptr && "[OsAllocator]: realloc failed");
    return r;
}

inline void
OsFree([[maybe_unused]] OsAllocator* s, void* p)
{
    ::free(p);
}

inline void
_OsFreeAll([[maybe_unused]] OsAllocator* s)
{
    assert(false && "[OsAllocator]: no 'freeAll()' method");
}

inline OsAllocator inl_OsAllocator {};
inline Allocator* inl_pOsAlloc = &inl_OsAllocator.super;

} /* namespace adt */

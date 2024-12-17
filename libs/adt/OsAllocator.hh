#pragma once

#include "IAllocator.hh"

#include <cassert>
#include <cstdlib>

namespace adt
{

/* default os allocator (aka malloc() / calloc() / realloc() / free()).
 * freeAll() method is not supported. */
struct OsAllocator
{
    IAllocator super {};

    OsAllocator([[maybe_unused]] u64 _ingnored = 0);

    [[nodiscard]] void* alloc(u64 mCount, u64 mSize);
    [[nodiscard]] void* zalloc(u64 mCount, u64 mSize);
    [[nodiscard]] void* realloc(void* ptr, u64 mCount, u64 mSize);
    void free(void* ptr);
    void freeAll(); /* assert(false) */
    void reset();
};

inline IAllocator*
OsAllocatorGet()
{
    static OsAllocator alloc {};
    return &alloc.super;
}

inline void*
OsAllocator::alloc(u64 mCount, u64 mSize)
{
    auto* r = ::malloc(mCount * mSize);
    assert(r != nullptr && "[OsAllocator]: calloc failed");
    return r;
}

inline void*
OsAllocator::zalloc(u64 mCount, u64 mSize)
{
    auto* r = ::calloc(mCount, mSize);
    assert(r != nullptr && "[OsAllocator]: calloc failed");
    return r;
}

inline void*
OsAllocator::realloc(void* p, u64 mCount, u64 mSize)
{
    auto* r = ::realloc(p, mCount * mSize);
    assert(r != nullptr && "[OsAllocator]: realloc failed");
    return r;
}

inline void
OsAllocator::free(void* p)
{
    ::free(p);
}

inline void
OsAllocator::freeAll()
{
    assert(false && "[OsAllocator]: no 'freeAll()' method");
}

inline const AllocatorVTable inl_OsAllocatorVTable {
    .alloc = decltype(AllocatorVTable::alloc)(+[](OsAllocator* s, u64 mCount, u64 mSize) {
        return s->alloc(mCount, mSize);
    }),
    .zalloc = decltype(AllocatorVTable::zalloc)(+[](OsAllocator* s, u64 mCount, u64 mSize) {
        return s->zalloc(mCount, mSize);
    }),
    .realloc = decltype(AllocatorVTable::realloc)(+[](OsAllocator* s, void* ptr, u64 mCount, u64 mSize) {
        return s->realloc(ptr, mCount, mSize);
    }),
    .free = decltype(AllocatorVTable::free)(+[](OsAllocator* s, void* ptr) {
        return s->free(ptr);
    }),
    .freeAll = decltype(AllocatorVTable::freeAll)(+[](OsAllocator* s) {
        return s->freeAll();
    }),
};

inline OsAllocator::OsAllocator(u64) : super(&inl_OsAllocatorVTable) {}

} /* namespace adt */

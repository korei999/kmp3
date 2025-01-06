#pragma once

#include "IAllocator.hh"

#include <cassert>
#include <cstdlib>

namespace adt
{

/* default os allocator (aka malloc() / calloc() / realloc() / free()).
 * freeAll() method is not supported. */
struct OsAllocator : IAllocator
{
    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize mCount, usize mSize) override final;
    void virtual free(void* ptr) override final;
    void virtual freeAll() override final; /* assert(false) */
};

inline OsAllocator*
OsAllocatorGet()
{
    static OsAllocator alloc {};
    return &alloc;
}

inline void*
OsAllocator::malloc(usize mCount, usize mSize)
{
    auto* r = ::malloc(mCount * mSize);
    assert(r != nullptr && "[OsAllocator]: malloc failed");
    return r;
}

inline void*
OsAllocator::zalloc(usize mCount, usize mSize)
{
    auto* r = ::calloc(mCount, mSize);
    assert(r != nullptr && "[OsAllocator]: calloc failed");
    return r;
}

inline void*
OsAllocator::realloc(void* p, usize mCount, usize mSize)
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

} /* namespace adt */

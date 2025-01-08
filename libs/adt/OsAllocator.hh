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
    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize mCount, usize mSize) noexcept(false) override final;
    void virtual free(void* ptr) noexcept override final;
    void virtual freeAll() noexcept override final; /* assert(false) */
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
    if (!r) throw AllocException("OsAllocator::malloc()");
    return r;
}

inline void*
OsAllocator::zalloc(usize mCount, usize mSize)
{
    auto* r = ::calloc(mCount, mSize);
    if (!r) throw AllocException("OsAllocator::zalloc()");
    return r;
}

inline void*
OsAllocator::realloc(void* p, usize mCount, usize mSize)
{
    auto* r = ::realloc(p, mCount * mSize);
    if (!r) throw AllocException("OsAllocator::realloc()");
    return r;
}

inline void
OsAllocator::free(void* p) noexcept
{
    ::free(p);
}

inline void
OsAllocator::freeAll() noexcept
{
    assert(false && "[OsAllocator]: no 'freeAll()' method");
}

} /* namespace adt */

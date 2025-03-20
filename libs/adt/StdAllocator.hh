#pragma once

#include "IAllocator.hh"

#include <cassert>
#include <cstdlib>

#ifdef ADT_USE_MIMALLOC
    #include "mimalloc.h"
#endif

namespace adt
{

/* default os allocator (aka malloc() / calloc() / realloc() / free()).
 * freeAll() method is not supported. */
struct StdAllocator : IAllocator
{
    static StdAllocator* inst();

    /* virtual */
    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false) override final;
    void virtual free(void* ptr) noexcept override final;
    ADT_WARN_LEAK void virtual freeAll() noexcept override final; /* assert(false) */
    /* virtual end */
};

inline StdAllocator*
StdAllocator::inst()
{
    static StdAllocator instance {};
    return &instance;
}

inline void*
StdAllocator::malloc(usize mCount, usize mSize)
{
#ifdef ADT_USE_MIMALLOC
    auto* r = ::mi_malloc(mCount * mSize);
#else
    auto* r = ::malloc(mCount * mSize);
#endif

    if (!r) throw AllocException("StdAllocator::malloc()");

    return r;
}

inline void*
StdAllocator::zalloc(usize mCount, usize mSize)
{
#ifdef ADT_USE_MIMALLOC
    auto* r = ::mi_zalloc(mCount * mSize);
#else
    auto* r = ::calloc(mCount, mSize);
#endif

    if (!r) throw AllocException("StdAllocator::zalloc()");

    return r;
}

inline void*
StdAllocator::realloc(void* p, usize, usize newCount, usize mSize)
{
#ifdef ADT_USE_MIMALLOC
    auto* r = ::mi_realloc(p, newCount * mSize);
#else
    auto* r = ::realloc(p, newCount * mSize);
#endif

    if (!r) throw AllocException("StdAllocator::realloc()");

    return r;
}

inline void
StdAllocator::free(void* p) noexcept
{
#ifdef ADT_USE_MIMALLOC
    ::mi_free(p);
#else
    ::free(p);
#endif
}

inline void
StdAllocator::freeAll() noexcept
{
    ADT_ASSERT(false, "no 'freeAll()' method");
}

} /* namespace adt */

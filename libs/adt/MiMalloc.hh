#pragma once

#include "IAllocator.hh"
#include "mimalloc.h"
#include "print.hh" /* IWYU pragma: keep */

#include <cstdlib>

namespace adt
{

/* Thread local general purpose allocator. */
struct MiMalloc : IAllocator
{
    static MiMalloc* inst();

    /* virtual */
    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false) override final;
    void virtual free(void* ptr) noexcept override final;
    ADT_WARN_LEAK void virtual freeAll() noexcept override final; /* assert(false) */
    /* virtual end */
};

inline MiMalloc*
MiMalloc::inst()
{
    static MiMalloc instance {};
    return &instance;
}

inline void*
MiMalloc::malloc(usize mCount, usize mSize)
{
    auto* r = ::mi_malloc(mCount * mSize);

    if (!r) throw AllocException("MiMalloc::malloc()");

    return r;
}

inline void*
MiMalloc::zalloc(usize mCount, usize mSize)
{
    auto* r = ::mi_calloc(mCount, mSize);

    if (!r) throw AllocException("MiMalloc::zalloc()");

    return r;
}

inline void*
MiMalloc::realloc(void* p, usize, usize newCount, usize mSize)
{
    auto* r = ::mi_reallocn(p, newCount, mSize);

    if (!r) throw AllocException("MiMalloc::realloc()");

    return r;
}

inline void
MiMalloc::free(void* p) noexcept
{
    ::mi_free(p);
}

inline void
MiMalloc::freeAll() noexcept
{
    ADT_ASSERT(false, "no 'freeAll()' method");
}

/* very fast general purpose, non thread safe, allocator. freeAll() is supported. */
struct MiHeap : IAllocator
{
    mi_heap_t* m_pHeap {};

    /* */

    MiHeap() = default;

    MiHeap(usize) : m_pHeap(mi_heap_new()) {}

    /* */

    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false) override final;
    void virtual free(void* ptr) noexcept override final;
    void virtual freeAll() noexcept override final;

    /* */

    void reset() noexcept;
};

inline void*
MiHeap::malloc(usize mCount, usize mSize)
{
    auto* r = ::mi_heap_mallocn(m_pHeap, mCount, mSize);

    if (!r) throw AllocException("MiHeap::malloc()");

    return r;
}

inline void*
MiHeap::zalloc(usize mCount, usize mSize)
{
    auto* r = ::mi_heap_zalloc(m_pHeap, mCount * mSize);

    if (!r) throw AllocException("MiHeap::zalloc()");

    return r;
}

inline void*
MiHeap::realloc(void* p, usize, usize newCount, usize mSize)
{
    auto* r = ::mi_reallocn(p, newCount, mSize);

    if (!r) throw AllocException("MiHeap::realloc()");

    return r;
}

inline void
MiHeap::free(void* ptr) noexcept
{
    ::mi_free(ptr);
}

inline void
MiHeap::freeAll() noexcept
{
    mi_heap_destroy(m_pHeap);
    *this = {};
}

inline void
MiHeap::reset() noexcept
{
    mi_heap_destroy(m_pHeap);
    m_pHeap = mi_heap_new();
}

} /* namespace adt */

#pragma once

#include "print.hh" /* IWYU pragma: keep */

#include <cstdlib>

#ifdef ADT_USE_MIMALLOC
    #include "mimalloc.h"
#endif

namespace adt
{

/* default os allocator (aka malloc() / calloc() / realloc() / free()).
 * freeAll() method is not supported. */
struct Gpa : IAllocator
{
    [[nodiscard]] static Gpa* inst(); /* nonnull */

    /* virtual */
    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false) override final;
    void virtual free(void* ptr) noexcept override final;
    [[nodiscard]] virtual constexpr bool doesFree() const noexcept override final { return true; }
    [[nodiscard]] virtual constexpr bool doesRealloc() const noexcept override final { return true; }
    /* virtual end */
};

/* non virtual */
struct GpaNV : AllocatorHelperCRTP<GpaNV>
{
    [[nodiscard]] static Gpa* inst() noexcept { return Gpa::inst(); }

    [[nodiscard]] static void* malloc(usize mCount, usize mSize) noexcept(false)
    { return Gpa::inst()->malloc(mCount, mSize); }

    [[nodiscard]] static void* zalloc(usize mCount, usize mSize) noexcept(false)
    { return Gpa::inst()->zalloc(mCount, mSize); }

    [[nodiscard]] static void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false)
    { return Gpa::inst()->realloc(ptr, oldCount, newCount, mSize); }

    static void free(void* ptr) noexcept
    { Gpa::inst()->free(ptr); }

    [[nodiscard]] static constexpr bool doesIndividualFree() noexcept { return true; }
};

inline Gpa*
Gpa::inst()
{
    static Gpa instance {};
    return &instance;
}

inline void*
Gpa::malloc(usize mCount, usize mSize)
{
#ifdef ADT_USE_MIMALLOC
    auto* r = ::mi_malloc(mCount * mSize);
#else
    auto* r = ::malloc(mCount * mSize);
#endif

    if (!r) [[unlikely]] throw AllocException("Gpa::malloc()");

    return r;
}

inline void*
Gpa::zalloc(usize mCount, usize mSize)
{
#ifdef ADT_USE_MIMALLOC
    auto* r = ::mi_zalloc(mCount * mSize);
#else
    auto* r = ::calloc(mCount, mSize);
#endif

    if (!r) [[unlikely]] throw AllocException("Gpa::zalloc()");

    return r;
}

inline void*
Gpa::realloc(void* p, usize, usize newCount, usize mSize)
{
#ifdef ADT_USE_MIMALLOC
    auto* r = ::mi_realloc(p, newCount * mSize);
#else
    auto* r = ::realloc(p, newCount * mSize);
#endif

    if (!r) [[unlikely]] throw AllocException("Gpa::realloc()");

    return r;
}

inline void
Gpa::free(void* p) noexcept
{
#ifdef ADT_USE_MIMALLOC
    ::mi_free(p);
#else
    ::free(p);
#endif
}

} /* namespace adt */

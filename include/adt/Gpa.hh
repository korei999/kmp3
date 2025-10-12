#pragma once

#include "print.hh" /* IWYU pragma: keep */

#include <cstdlib>

#ifdef ADT_USE_MIMALLOC
    #include "mimalloc.h"
#endif

namespace adt
{

/* Libc allocator (aka malloc() / calloc() / realloc() / free()). */
struct Gpa final : IAllocator
{
    [[nodiscard]] static Gpa* inst(); /* nonnull */

    /* virtual */
    [[nodiscard]] virtual void* malloc(usize nBytes) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize nBytes) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldNBytes, usize newNBytes) noexcept(false) override final;
    void virtual free(void* ptr, usize nBytes) noexcept override final;

    [[nodiscard]] virtual constexpr bool doesFree() const noexcept override final { return true; }
    [[nodiscard]] virtual constexpr bool doesRealloc() const noexcept override final { return true; }
    /* virtual end */

    static void free(void* ptr) noexcept;
};

/* non virtual */
struct GpaNV final : AllocatorHelperCRTP<GpaNV>
{
    [[nodiscard]] static Gpa* inst() noexcept { return Gpa::inst(); }

    [[nodiscard]] static void* malloc(usize nBytes) noexcept(false)
    { return Gpa::inst()->malloc(nBytes); }

    [[nodiscard]] static void* zalloc(usize nBytes) noexcept(false)
    { return Gpa::inst()->zalloc(nBytes); }

    [[nodiscard]] static void* realloc(void* ptr, usize oldNBytes, usize newNBytes) noexcept(false)
    { return Gpa::inst()->realloc(ptr, oldNBytes, newNBytes); }

    static void free(void* ptr, usize nBytes) noexcept
    { Gpa::inst()->free(ptr, nBytes); }

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
Gpa::malloc(usize nBytes)
{
#ifdef ADT_USE_MIMALLOC
    auto* r = ::mi_malloc(nBytes);
#else
    auto* r = ::malloc(nBytes);
#endif

    if (!r) [[unlikely]] throw AllocException("Gpa::malloc()");

    return r;
}

inline void*
Gpa::zalloc(usize nBytes)
{
#ifdef ADT_USE_MIMALLOC
    auto* r = ::mi_zalloc(nBytes);
#else
    auto* r = ::calloc(1, nBytes);
#endif

    if (!r) [[unlikely]] throw AllocException("Gpa::zalloc()");

    return r;
}

inline void*
Gpa::realloc(void* p, usize, usize newNBytes)
{
#ifdef ADT_USE_MIMALLOC
    auto* r = ::mi_realloc(p, newNBytes);
#else
    auto* r = ::realloc(p, newNBytes);
#endif

    if (!r) [[unlikely]] throw AllocException("Gpa::realloc()");

    return r;
}

inline void
Gpa::free(void* p, usize) noexcept
{
#ifdef ADT_USE_MIMALLOC
    ::mi_free(p);
#else
    ::free(p);
#endif
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

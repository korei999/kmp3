#pragma once

#include "IAllocator.hh"
#include "mimalloc.h"

namespace adt
{

/* Thread safe general purpose allocator. */
struct MiMalloc : IAllocator
{
    static MiMalloc* inst();

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

struct MiMallocNV : AllocatorHelperCRTP<MiMallocNV>
{
    [[nodiscard]] static MiMalloc* inst() noexcept { return MiMalloc::inst(); }

    [[nodiscard]] static void* malloc(usize nBytes) noexcept(false)
    { return MiMalloc::inst()->malloc(nBytes); }

    [[nodiscard]] static void* zalloc(usize nBytes) noexcept(false)
    { return MiMalloc::inst()->zalloc(nBytes); }

    [[nodiscard]] static void* realloc(void* ptr, usize oldNBytes, usize newNBytes) noexcept(false)
    { return MiMalloc::inst()->realloc(ptr, oldNBytes, newNBytes); }

    static void free(void* ptr, usize nBytes) noexcept
    { MiMalloc::inst()->free(ptr, nBytes); }

    static void free(void* ptr) noexcept
    { MiMalloc::inst()->free(ptr); }

    [[nodiscard]] static constexpr bool doesIndividualFree() noexcept { return true; }
};

inline MiMalloc*
MiMalloc::inst()
{
    static MiMalloc instance {};
    return &instance;
}

inline void*
MiMalloc::malloc(usize nBytes)
{
    auto* r = ::mi_malloc(nBytes);
    if (!r) [[unlikely]] throw AllocException("MiMalloc::malloc()");

    return r;
}

inline void*
MiMalloc::zalloc(usize nBytes)
{
    auto* r = ::mi_zalloc(nBytes);
    if (!r) [[unlikely]] throw AllocException("MiMalloc::zalloc()");

    return r;
}

inline void*
MiMalloc::realloc(void* p, usize, usize newNBytes)
{
    auto* r = ::mi_realloc(p, newNBytes);
    if (!r) [[unlikely]] throw AllocException("MiMalloc::realloc()");

    return r;
}

inline void
MiMalloc::free(void* p, usize) noexcept
{
    ::mi_free(p);
}

inline void
MiMalloc::free(void* ptr) noexcept
{
    ::mi_free(ptr);
}

/* very fast general purpose, non thread safe, allocator. freeAll() is supported. */
struct MiHeap : IArena
{
    mi_heap_t* m_pHeap {};

    /* */

    MiHeap() = default;

    MiHeap(InitFlag) : m_pHeap(mi_heap_new()) {}
    MiHeap(usize) : m_pHeap(mi_heap_new()) {}

    /* */

    [[nodiscard]] virtual void* malloc(usize nBytes) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize nBytes) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldNBytes, usize newNBytes) noexcept(false) override final;
    void virtual free(void* ptr, usize nBytes) noexcept override final;
    void virtual freeAll() noexcept override final;
    [[nodiscard]] virtual constexpr bool doesFree() const noexcept override final { return true; }
    [[nodiscard]] virtual constexpr bool doesRealloc() const noexcept override final { return true; }

    /* */

    void reset() noexcept;

    /* */

    static void free(void* ptr) noexcept;
};

inline void*
MiHeap::malloc(usize nBytes)
{
    auto* r = ::mi_heap_malloc(m_pHeap, nBytes);
    if (!r) [[unlikely]] throw AllocException("MiHeap::malloc()");

    return r;
}

inline void*
MiHeap::zalloc(usize nBytes)
{
    auto* r = ::mi_heap_zalloc(m_pHeap, nBytes);
    if (!r) [[unlikely]] throw AllocException("MiHeap::zalloc()");

    return r;
}

inline void*
MiHeap::realloc(void* p, usize, usize newNBytes)
{
    auto* r = ::mi_realloc(p, newNBytes);
    if (!r) [[unlikely]] throw AllocException("MiHeap::realloc()");

    return r;
}

inline void
MiHeap::free(void* ptr, usize) noexcept
{
    ::mi_free(ptr);
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

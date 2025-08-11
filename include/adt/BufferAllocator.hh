#pragma once

#include "IAllocator.hh"
#include "Span.hh" /* IWYU pragma: keep */
#include "print.hh" /* IWYU pragma: keep */

#include <cstring>

namespace adt
{

/* Bump allocator, using fixed size memory buffer. */
struct BufferAllocator : public IArena
{
    u8* m_pMemBuffer = nullptr;
    usize m_size = 0;
    usize m_cap = 0;
    void* m_pLastAlloc = nullptr;
    usize m_lastAllocSize = 0;
    
    /* */

    constexpr BufferAllocator() = default;

    constexpr BufferAllocator(u8* pMemory, usize capacity) noexcept
        : m_pMemBuffer {pMemory},
          m_cap {capacity} {}

    template<typename T, isize N>
    BufferAllocator(T (&aMem)[N]) noexcept
        : m_pMemBuffer {reinterpret_cast<u8*>(aMem)},
          m_cap {N * sizeof(T)} {}

    template<typename T>
    constexpr BufferAllocator(Span<T> sp) noexcept
        : BufferAllocator {reinterpret_cast<u8*>(sp.data()), sp.size() * sizeof(T)} {}

    /* */

    [[nodiscard]] virtual constexpr void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual constexpr void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual constexpr void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false) override final;
    [[deprecated("noop")]] constexpr virtual void free(void*) noexcept override final { /* noop */ }
    constexpr virtual void freeAll() noexcept override final; /* same as reset */
    constexpr void reset() noexcept;
    constexpr isize realCap() const noexcept;
};

inline constexpr void*
BufferAllocator::malloc(usize mCount, usize mSize)
{
    usize realSize = alignUp8(mCount * mSize);

    if (m_size + realSize > m_cap)
    {
        errno = ENOBUFS;
        throw AllocException("BufferAllocator::malloc(): out of memory");
    }

    void* ret = &m_pMemBuffer[m_size];
    m_size += realSize;
    m_pLastAlloc = ret;
    m_lastAllocSize = realSize;

    return ret;
}

inline constexpr void*
BufferAllocator::zalloc(usize mCount, usize mSize)
{
    auto* p = malloc(mCount, mSize);
    memset(p, 0, mCount*mSize);
    return p;
}

inline constexpr void*
BufferAllocator::realloc(void* p, usize oldCount, usize newCount, usize mSize)
{
    if (!p) return malloc(newCount, mSize);

    ADT_ASSERT(p >= m_pMemBuffer && p < m_pMemBuffer + m_cap, "invalid pointer");

    const usize realSize = alignUp8(newCount * mSize);

    if ((m_size + realSize - m_lastAllocSize) > m_cap)
    {
        errno = ENOBUFS;
        throw AllocException("BufferAllocator::realloc(): out of memory");
    }

    if (p == m_pLastAlloc)
    {
        m_size -= m_lastAllocSize;
        m_size += realSize;
        m_lastAllocSize = realSize;

        return p;
    }
    else
    {
        if (newCount <= oldCount) return p;

        auto* ret = malloc(newCount, mSize);
        memcpy(ret, p, oldCount);

        return ret;
    }
}

inline constexpr void
BufferAllocator::freeAll() noexcept
{
    reset();
}

inline constexpr void
BufferAllocator::reset() noexcept
{
    m_size = 0;
    m_pLastAlloc = nullptr;
}

inline constexpr isize
BufferAllocator::realCap() const noexcept
{
    return alignDown8(m_cap);
}

} /* namespace adt */

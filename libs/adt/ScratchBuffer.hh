#pragma once

#include "IAllocator.hh"
#include "Span.hh" /* IWYU pragma: keep */

#include <cstring>

namespace adt
{

/* Each nextMem() invalidates previous nextMem() span. */
struct ScratchBuffer
{
    Span<u8> m_sp {};
    ssize m_pos {};
    ssize m_typeCap {};

    /* */

    ScratchBuffer() = default;

    template<typename T>
    ScratchBuffer(Span<T> sp) noexcept
        : m_sp((u8*)sp.data(), sp.size() * sizeof(T)), m_typeCap(calcTypeCap<T>()) {}

    template<typename T, usize SIZE>
    ScratchBuffer(T (&aBuff)[SIZE]) noexcept
        : m_sp((u8*)aBuff, SIZE * sizeof(T)), m_typeCap(calcTypeCap<T>()) {}

    template<typename T>
    ScratchBuffer(T* pMem, ssize size) noexcept
        : m_sp((u8*)pMem, size), m_typeCap(calcTypeCap<T>()) {}

    /* */

    template<typename T>
    [[nodiscard]] Span<T> nextMem(ssize mCount) noexcept;

    /* nextMem() + memset() to zero */
    template<typename T>
    [[nodiscard]] Span<T> nextMemZero(ssize mCount) noexcept;

    template<typename T>
    [[nodiscard]] Span<T> allMem() noexcept;

    void zeroOut() noexcept;

    ssize cap() const noexcept { return m_sp.size(); }

protected:
    template<typename T>
    ssize calcTypeCap() const noexcept { return static_cast<ssize>(cap() / sizeof(T)); }
};


template<typename T>
inline Span<T>
ScratchBuffer::nextMem(ssize mCount) noexcept
{
    ADT_ASSERT(m_sp.size() > 0, "empty");

    const ssize realSize = align8(mCount * sizeof(T));

    if (realSize >= m_sp.size())
    {
        print::err("ScratchBuffer::nextMem(): allocating more than capacity ({} < {}), returing full buffer\n", m_sp.size(), realSize);
        return {(T*)m_sp.data(), calcTypeCap<T>()};
    }
    else if (realSize + m_pos > m_sp.size())
    {
        m_pos = 0;
    }

    void* pMem = &m_sp[m_pos];
    m_pos += realSize;

    return {(T*)pMem, mCount};
}

template<typename T>
inline Span<T>
ScratchBuffer::nextMemZero(ssize mCount) noexcept
{
    auto sp = nextMem<T>(mCount);
    memset(sp.data(), 0, sp.size() * sizeof(T));
    return sp;
}

template<typename T>
Span<T>
ScratchBuffer::allMem() noexcept
{
    return {reinterpret_cast<T*>(m_sp.data()), calcTypeCap<T>()};
}

inline void
ScratchBuffer::zeroOut() noexcept
{
    m_pos = 0;
    memset(m_sp.data(), 0, m_sp.size());
}

} /* namespace adt */

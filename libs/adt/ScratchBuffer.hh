#pragma once

#include "Span.hh" /* IWYU pragma: keep */

#include <cstring>

namespace adt
{

/* Each nextMem() invalidates previous nextMem() span. */
struct ScratchBuffer
{
    Span<u8> m_sp {};
    isize m_pos {};
#ifndef NDEBUG
    bool m_bTaken {};
#endif

    /* */

    ScratchBuffer() = default;

    template<typename T>
    ScratchBuffer(Span<T> sp) noexcept
        : m_sp {reinterpret_cast<u8*>(sp.data()), isize(sp.size() * sizeof(T))} {}

    template<typename T, usize SIZE>
    ScratchBuffer(T (&aBuff)[SIZE]) noexcept
        : m_sp {reinterpret_cast<u8*>(aBuff), isize(SIZE * sizeof(T))} {}

    template<typename T>
    ScratchBuffer(T* pMem, isize size) noexcept
        : m_sp {reinterpret_cast<u8*>(pMem), size} {}

    /* */

    /* nextMem() + memset() to zero */
    template<typename T>
    [[nodiscard]] Span<T> nextMemZero(isize mCount) noexcept;

    template<typename T>
    [[nodiscard]] Span<T> nextMem() noexcept;

    void reset() noexcept;

    void zeroOut() noexcept;

    isize cap() const noexcept { return m_sp.size(); }

protected:
    template<typename T>
    isize calcTypeCap() const noexcept { return static_cast<isize>(cap() / sizeof(T)); }
};


template<typename T>
inline Span<T>
ScratchBuffer::nextMemZero(isize mCount) noexcept
{
    auto sp = nextMem<T>();
    memset(sp.data(), 0, mCount * sizeof(T));
    return sp;
}

template<typename T>
Span<T>
ScratchBuffer::nextMem() noexcept
{
#ifndef NDEBUG
    ADT_ASSERT(m_bTaken != true, "must reset() between nextMem() calls");
    m_bTaken = true;
#endif
    return {reinterpret_cast<T*>(m_sp.data()), calcTypeCap<T>()};
}

inline void
ScratchBuffer::zeroOut() noexcept
{
    m_pos = 0;
    memset(m_sp.data(), 0, m_sp.size());
}

inline void
ScratchBuffer::reset() noexcept
{
#ifndef NDEBUG
    m_bTaken = false;
#endif
}

} /* namespace adt */

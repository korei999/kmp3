#pragma once

#include "Span.hh" /* IWYU pragma: keep */

#include <cstring>

#define ADT_SCRATCH_NEXT_MEM_ZERO(self, type, mCount) (self)->nextMemZero<type>(mCount, __FILE__, __LINE__)
#define ADT_SCRATCH_NEXT_MEM(self, type) (self)->nextMem<type>(__FILE__, __LINE__)

namespace adt
{

/* Each nextMem() invalidates previous nextMem() span. */
struct ScratchBuffer
{
    Span<u8> m_sp {};
    isize m_pos {};

#ifndef NDEBUG
    bool m_bTaken {};
    const char* m_ntsFile {};
    int m_line {};
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
    [[nodiscard]] Span<T> nextMemZero(isize mCount, const char* ntsFile = "", int line = 0) noexcept;

    template<typename T>
    [[nodiscard]] Span<T> nextMem(const char* ntsFile = "", int line = 0) noexcept;

    void reset() noexcept;

    void zeroOut() noexcept;

    isize cap() const noexcept { return m_sp.size(); }

protected:
    template<typename T>
    isize calcTypeCap() const noexcept { return static_cast<isize>(cap() / sizeof(T)); }
};


template<typename T>
inline Span<T>
ScratchBuffer::nextMemZero(isize mCount, const char* ntsFile, int line) noexcept
{
    auto sp = nextMem<T>(ntsFile, line);
    memset(sp.data(), 0, mCount * sizeof(T));
    return sp;
}

template<typename T>
Span<T>
ScratchBuffer::nextMem([[maybe_unused]] const char* ntsFile, [[maybe_unused]] int line) noexcept
{
#ifndef NDEBUG
    ADT_ASSERT(m_bTaken != true, "must reset() between nextMem() calls. Prev call: [{}, {}]", m_ntsFile, m_line);
    m_bTaken = true;
    m_ntsFile = ntsFile;
    m_line = line;
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

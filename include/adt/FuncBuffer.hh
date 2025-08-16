#pragma once

#include "assert.hh"

namespace adt
{

template<typename R, int SIZE = 8>
struct FuncBuffer
{
    static_assert(SIZE >= 8);

    /* */

    R (*m_pfn)(void* p);
    u8 m_aArgBuff[SIZE];

    /* */

    FuncBuffer() noexcept : m_pfn {}, m_aArgBuff {} {}
    FuncBuffer(UninitFlag) noexcept {}

    template<typename CL> requires (sizeof(CL) <= SIZE)
    FuncBuffer(const CL& cl) noexcept;

    template<typename T> requires (sizeof(T) <= SIZE)
    FuncBuffer(R (*pfn)(void*), T arg) noexcept;

    FuncBuffer(R (*pfn)(void*), void* pArg, isize argSize) noexcept;

    /* */

    explicit operator bool() const noexcept { return bool(m_pfn); }

    [[nodiscard]] R operator()() const;
};

template<typename R, int SIZE>
inline R
FuncBuffer<R, SIZE>::operator()() const
{
    return m_pfn((void*)(m_aArgBuff));
}

template<typename R, int SIZE>
template<typename CL>
requires (sizeof(CL) <= SIZE)
inline
FuncBuffer<R, SIZE>::FuncBuffer(const CL& cl) noexcept
    : m_pfn {
        [](void* p) -> R
        {
            return static_cast<CL*>(p)->operator()();
        }
    },
      m_aArgBuff {}
{
    static_assert(std::is_same_v<decltype(cl()), R>, "fix lambda's return value");

    new(m_aArgBuff) CL {cl};
}

template<typename R, int SIZE>
template<typename T>
requires (sizeof(T) <= SIZE)
inline
FuncBuffer<R, SIZE>::FuncBuffer(R (*pfn)(void*), T arg) noexcept
    : m_pfn {pfn}, m_aArgBuff {}
{
    ADT_ASSERT(pfn != nullptr, "");

    new(m_aArgBuff) T {arg};
}

template<typename R, int SIZE>
inline
FuncBuffer<R, SIZE>::FuncBuffer(R (*pfn)(void*), void* pArg, isize argSize) noexcept
    : m_pfn {pfn}, m_aArgBuff {}
{
    ADT_ASSERT(pfn != nullptr, "");
    ADT_ASSERT(argSize <= SIZE, "can't fit, argSize: {}, SIZE; {}\n", argSize, SIZE);

    ::memcpy(m_aArgBuff, pArg, argSize);
}

} /* namespace adt */

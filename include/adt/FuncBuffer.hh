#pragma once

#include "types.hh"

#include <type_traits>

namespace adt
{

template<typename R, int SIZE = 8>
struct FuncBuffer
{
    static_assert(SIZE >= 8);

    /* */

    R (*m_pfn)(void* p) {};
    u8 m_aArgBuff[SIZE] {};

    /* */

    FuncBuffer() noexcept = default;

    template<typename CL> requires (sizeof(CL) <= SIZE)
    FuncBuffer(const CL& cl) noexcept;

    template<typename T> requires (sizeof(T) <= SIZE)
    FuncBuffer(R (*pfn)(void*), T arg) noexcept;

    /* */

    [[nodiscard]] R operator()();
};

template<typename R, int SIZE>
inline R
FuncBuffer<R, SIZE>::operator()()
{
    return m_pfn(reinterpret_cast<void*>(m_aArgBuff));
}

template<typename R, int SIZE>
template<typename CL>
requires (sizeof(CL) <= SIZE)
inline
FuncBuffer<R, SIZE>::FuncBuffer(const CL& cl) noexcept
    : m_pfn
    {
        [](void* p) -> R {
            return static_cast<CL*>(p)->operator()();
        }
    }
{
    static_assert(std::is_same_v<decltype(cl()), R>, "fix lambda's return value");

    new(m_aArgBuff) CL {cl};
}

template<typename R, int SIZE>
template<typename T>
requires (sizeof(T) <= SIZE)
inline
FuncBuffer<R, SIZE>::FuncBuffer(R (*pfn)(void*), T arg) noexcept
    : m_pfn {pfn}
{
    new(m_aArgBuff) T {arg};
}

} /* namespace adt */

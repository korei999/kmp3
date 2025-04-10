#pragma once

#include "SpanDecl.hh"

#include "types.hh"
#include "assert.hh"

namespace adt
{

template<typename T>
inline constexpr ssize
Span<T>::lastI() const noexcept
{
    ADT_ASSERT(m_size > 0, "empty: size: {}", m_size);
    return m_size - 1;
}

#define ADT_RANGE_CHECK ADT_ASSERT(i >= 0 && i < m_size, "i: {}, m_size: {}", i, m_size);

template<typename T>
inline constexpr ssize
Span<T>::idx(const T* pItem) const noexcept
{
    ssize i = pItem - m_pData;
    ADT_RANGE_CHECK
        return i;
}

template<typename T>
inline constexpr T&
Span<T>::operator[](ssize i) noexcept
{
    ADT_RANGE_CHECK
        return m_pData[i];
}

template<typename T>
inline constexpr const T&
Span<T>::operator[](ssize i) const noexcept
{
    ADT_RANGE_CHECK
        return m_pData[i];
}

#undef ADT_RANGE_CHECK

} /* namespace adt */

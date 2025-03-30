#pragma once

#include "types.hh"
#include "assert.hh"

namespace adt
{

template<typename T>
struct Span
{
    T* m_pData {};
    ssize m_size {};

    /* */

    constexpr Span() = default;

    constexpr Span(const T* pData, ssize size) noexcept
        : m_pData(const_cast<T*>(pData)), m_size(size) {}

    template<ssize N>
    constexpr Span(T (&aChars)[N]) noexcept
        : m_pData(aChars), m_size(N) {}

    /* */

    constexpr bool empty() const { return m_size <= 0; }

    constexpr T* data() noexcept { return m_pData; }
    constexpr const T* data() const noexcept { return m_pData; }

    constexpr ssize size() const noexcept { return m_size; }

    constexpr ssize lastI() const noexcept { ADT_ASSERT(m_size > 0, "empty: size: %llu", m_size); return m_size - 1; }

    constexpr T& first() noexcept { return operator[](0); }
    constexpr const T& first() const noexcept { return operator[](0); }

    constexpr T& last() noexcept { return operator[](m_size - 1); }
    constexpr const T& last() const noexcept { return operator[](m_size - 1); }

#define ADT_RANGE_CHECK ADT_ASSERT(i >= 0 && i < m_size, "i: %lld, m_size: %lld", i, m_size);

    constexpr ssize
    idx(const T* pItem) const noexcept
    {
        ssize i = pItem - m_pData;
        ADT_RANGE_CHECK
        return i;
    }

    constexpr T&
    operator[](ssize i) noexcept
    {
        ADT_RANGE_CHECK
        return m_pData[i];
    }

    constexpr const T&
    operator[](ssize i) const noexcept
    {
        ADT_RANGE_CHECK
        return m_pData[i];
    }

#undef ADT_RANGE_CHECK

    constexpr operator bool() const { return m_pData != nullptr; }

    /* */

    struct It
    {
        T* s;

        It(T* pFirst) : s{pFirst} {}

        T& operator*() noexcept { return *s; }
        T* operator->() noexcept { return s; }

        It operator++() noexcept { ++s; return *this; }
        It operator++(int) noexcept { T* tmp = s++; return tmp; }

        It operator--() noexcept { --s; return *this; }
        It operator--(int) noexcept { T* tmp = s--; return tmp; }

        friend constexpr bool operator==(const It& l, const It& r) noexcept { return l.s == r.s; }
        friend constexpr bool operator!=(const It& l, const It& r) noexcept { return l.s != r.s; }
    };

    It begin()  noexcept { return {&m_pData[0]}; }
    It end()    noexcept { return {&m_pData[m_size]}; }
    It rbegin() noexcept { return {&m_pData[m_size - 1]}; }
    It rend()   noexcept { return {m_pData - 1}; }

    const It begin() const  noexcept { return {&m_pData[0]}; }
    const It end() const    noexcept { return {&m_pData[m_size]}; }
    const It rbegin() const noexcept { return {&m_pData[m_size - 1]}; }
    const It rend() const   noexcept { return {m_pData - 1}; }
};

} /* namespace adt */

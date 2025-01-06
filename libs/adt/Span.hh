#pragma once

#include "types.hh"

#include <cassert>

namespace adt
{

template<typename T>
struct Span
{
    T* m_pData {};
    ssize m_size {};

    /* */

    constexpr Span() = default;
    constexpr Span(T* pData, ssize size) : m_pData(pData), m_size(size) {}

    /* */

    constexpr T* data() { return m_pData; }
    constexpr const T* data() const { return m_pData; }

    constexpr ssize getSize() const { return m_size; }

    constexpr ssize lastI() const { return m_size - 1; }

    constexpr T&
    operator[](ssize i)
    {
        assert(i < m_size && "[Span]: out of range");
        return m_pData + i;
    }

    constexpr T&
    operator[](ssize i) const
    {
        assert(i < m_size && "[Span]: out of range");
        return m_pData + i;
    }

    /* */

    struct It
    {
        T* s;

        It(T* pFirst) : s{pFirst} {}

        T& operator*() { return *s; }
        T* operator->() { return s; }

        It operator++() { ++s; return *this; }
        It operator++(int) { T* tmp = s++; return tmp; }

        It operator--() { --s; return *this; }
        It operator--(int) { T* tmp = s--; return tmp; }

        friend constexpr bool operator==(const It& l, const It& r) { return l.s == r.s; }
        friend constexpr bool operator!=(const It& l, const It& r) { return l.s != r.s; }
    };

    It begin() { return {&m_pData[0]}; }
    It end() { return {&m_pData[m_size]}; }
    It rbegin() { return {&m_pData[m_size - 1]}; }
    It rend() { return {m_pData - 1}; }

    const It begin() const { return {&m_pData[0]}; }
    const It end() const { return {&m_pData[m_size]}; }
    const It rbegin() const { return {&m_pData[m_size - 1]}; }
    const It rend() const { return {m_pData - 1}; }
};

} /* namespace adt */

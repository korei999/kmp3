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

    template<ssize N>
    constexpr Span(T (&aChars)[N]) : m_pData(aChars), m_size(N) {}

    /* */

    constexpr T* data() { return m_pData; }
    constexpr const T* data() const { return m_pData; }

    constexpr ssize getSize() const { return m_size; }

    constexpr ssize lastI() const { assert(m_size > 0 && "[Span]: empty"); return m_size - 1; }

    constexpr ssize
    idx(const T* pItem) const
    {
        ssize i = pItem - m_pData;
        assert(i >= 0 && i < m_size && "[Span]: out of range");
        return i;
    }

    constexpr T&
    operator[](ssize i)
    {
        assert(i >= 0 && i < m_size && "[Span]: out of range");
        return m_pData[i];
    }

    constexpr const T&
    operator[](ssize i) const
    {
        assert(i >= 0 && i < m_size && "[Span]: out of range");
        return m_pData[i];
    }

    constexpr operator bool() const { return m_pData != nullptr; }

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

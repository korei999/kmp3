#pragma once

#include "assert.hh"

#include <utility>

namespace adt
{

template<typename T>
struct Opt
{
    T m_data {};
    bool m_bHasValue = false;

    /* */

    constexpr Opt() = default;
    constexpr Opt(const T& x)
    {
        m_data = x;
        m_bHasValue = true;
    }

    /* */

    explicit constexpr operator bool() const { return m_bHasValue; }

    /* */

    constexpr T& value() { ADT_ASSERT(m_bHasValue, "no value"); return m_data; }
    constexpr const T& value() const { ADT_ASSERT(m_bHasValue, "no value"); return m_data; }

    constexpr T& valueOrZero() { return m_data; }
    constexpr const T& valueOrZero() const { return m_data; }

    constexpr T
    valueOr(T&& v) const
    {
        if (m_bHasValue) return m_data;
        else return std::forward<T>(v);
    }
};

} /* namespace adt */

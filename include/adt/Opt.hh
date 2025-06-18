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
    constexpr Opt(const T& x) : m_data {x}, m_bHasValue {true} {}

    constexpr Opt(T&& x) : m_data {std::move(x)}, m_bHasValue {true} {}

    /* */

    explicit constexpr operator bool() const { return m_bHasValue; }

    /* */

    constexpr T& value() { ADT_ASSERT(m_bHasValue, "no value"); return m_data; }
    constexpr const T& value() const { ADT_ASSERT(m_bHasValue, "no value"); return m_data; }

    constexpr T& valueOrEmpty() { return m_data; }
    constexpr const T& valueOrEmpty() const { return m_data; }

    constexpr T
    valueOr(const T& v) const
    {
        return valueOrEmplace(v);
    }

    constexpr T
    valueOr(T&& v) const
    {
        return valueOrEmplace(std::move(v));
    }

    template<typename ...ARGS>
    constexpr T
    valueOrEmplace(ARGS&&... args) const
    {
        if (m_bHasValue) return m_data;
        else return T (std::forward<ARGS>(args)...);
    }
};

} /* namespace adt */

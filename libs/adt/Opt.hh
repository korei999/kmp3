#pragma once

#include <cassert>
#include <new> /* IWYU pragma: keep */

namespace adt
{

template<typename T>
struct Opt
{
    T m_data {};
    bool m_bHasValue = false;

    /* */

    constexpr Opt() = default;
    constexpr Opt(const T& x, bool bHasValue = true)
    {
        m_data = x;
        m_bHasValue = bHasValue;
    }

    /* */

    constexpr T& value() { assert(m_bHasValue && "[Opt]: has no value"); return m_data; }

    constexpr operator bool() const { return m_bHasValue; }
};

} /* namespace adt */

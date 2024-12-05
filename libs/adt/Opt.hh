#pragma once

namespace adt
{

template<typename T>
struct Opt
{
    T data {};
    bool bHasValue = false;

    constexpr Opt() = default;

    constexpr Opt(const T& x, bool _bHasValue = true)
    {
        bHasValue = _bHasValue;
        data = x;
    }

    constexpr explicit operator bool() const
    {
        return this->bHasValue;
    }
};

} /* namespace adt */

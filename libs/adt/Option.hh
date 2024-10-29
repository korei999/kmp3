#pragma once

namespace adt
{

template<typename T>
struct Option
{
    bool bHasValue = false;
    T data {};

    constexpr Option() = default;

    constexpr Option(const T& x, bool _bHasValue = true)
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

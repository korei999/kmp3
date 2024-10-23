#pragma once

#include "types.hh"

namespace adt
{

enum RESULT : u8 { SUCCES = 0, FAILURE };

template<typename T, typename E = RESULT>
struct Result
{
    T data;
    E eCode;

    constexpr explicit operator bool() const
    {
        return int(eCode) == 0;
    }
};

} /* namespace adt */

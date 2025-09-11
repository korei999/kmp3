#pragma once

#include "print-inl.hh"

namespace adt
{

template<typename A, typename B>
struct Pair
{
    ADT_NO_UNIQUE_ADDRESS A first {};
    ADT_NO_UNIQUE_ADDRESS B second {};

    /* */

    constexpr bool operator==(const Pair<A, B>&) const = default;
};

template<typename A, typename B>
Pair(A&&, B&&) -> Pair<A, B>;

namespace print
{

template<typename A, typename B>
inline u32
format(Context* pCtx, FormatArgs fmtArgs, const Pair<A, B>& x)
{
    fmtArgs.eFmtFlags |= FormatArgs::FLAGS::PARENTHESES;
    return formatVariadic(pCtx, fmtArgs, x.first, x.second);
}

} /* namespace print */

} /* namespace adt */

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

    bool operator==(const Pair<A, B>&) const = default;
};

template<typename A, typename B>
Pair(A&&, B&&) -> Pair<A, B>;

namespace print
{

template<typename A, typename B>
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const Pair<A, B>& x)
{
    return pCtx->pBuilder->pushFmt(pFmtArgs, "({}, {})", x.first, x.second);
}

} /* namespace print */

} /* namespace adt */

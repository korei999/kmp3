#pragma once

#include "print.hh"

namespace adt
{

template<typename A, typename B>
struct Pair
{
    ADT_NO_UNIQUE_ADDRESS A first {};
    ADT_NO_UNIQUE_ADDRESS B second {};
};

template<typename A, typename B>
constexpr bool
operator==(const Pair<A, B>& l, const Pair<A, B>& r)
{
    return l.first == r.first && l.second == r.second;
}

template<typename A, typename B>
constexpr bool
operator!=(const Pair<A, B>& l, const Pair<A, B>& r)
{
    return !(l == r);
}

template<typename A, typename B>
constexpr bool
operator<(const Pair<A, B>& l, const Pair<A, B>& r)
{
    return l.first < r.first && l.second < r.second;
}

template<typename A, typename B>
constexpr bool
operator>(const Pair<A, B>& l, const Pair<A, B>& r)
{
    return l.first > r.first && l.second > r.second;
}

template<typename A, typename B>
constexpr bool
operator<=(const Pair<A, B>& l, const Pair<A, B>& r)
{
    return l.first <= r.first && l.second <= r.second;
}

template<typename A, typename B>
constexpr bool
operator>=(const Pair<A, B>& l, const Pair<A, B>& r)
{
    return l.first >= r.first && l.second >= r.second;
}

namespace print
{

template<typename A, typename B>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const Pair<A, B>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.first, x.second);
}

} /* namespace print */

} /* namespace adt */

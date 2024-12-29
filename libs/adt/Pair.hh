#pragma once

#include "print.hh"

namespace adt
{

template<typename A, typename B>
struct Pair
{
    A first {};
    B second {};
};

template<typename A, typename B>
constexpr bool
operator==(const Pair<A, B>& l, const Pair<A, B>& r)
{
    auto& [lFirst, lSecond] = l;
    auto& [rFirst, rSecond] = r;

    return lFirst == rFirst && lSecond == rSecond;
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
    auto& [lFirst, lSecond] = l;
    auto& [rFirst, rSecond] = r;

    return lFirst < rFirst && lSecond < rSecond;
}

template<typename A, typename B>
constexpr bool
operator>(const Pair<A, B>& l, const Pair<A, B>& r)
{
    auto& [lFirst, lSecond] = l;
    auto& [rFirst, rSecond] = r;

    return lFirst > rFirst && lSecond > rSecond;
}

template<typename A, typename B>
constexpr bool
operator<=(const Pair<A, B>& l, const Pair<A, B>& r)
{
    auto& [lFirst, lSecond] = l;
    auto& [rFirst, rSecond] = r;

    return lFirst <= rFirst && lSecond <= rSecond;
}

template<typename A, typename B>
constexpr bool
operator>=(const Pair<A, B>& l, const Pair<A, B>& r)
{
    auto& [lFirst, lSecond] = l;
    auto& [rFirst, rSecond] = r;

    return lFirst >= rFirst && lSecond >= rSecond;
}

namespace print
{

template<typename A, typename B>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const Pair<A, B>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    auto& [first, second] = x;
    return printArgs(ctx, first, second);
}

} /* namespace print */

} /* namespace adt */

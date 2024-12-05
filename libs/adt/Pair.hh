#pragma once

#include "print.hh"

namespace adt
{

template<typename A, typename B>
struct DefaultPair
{
    A first {};
    B second {};
};

template<typename A, typename B, template<typename, typename> typename C = DefaultPair>
struct Pair : C<A, B>
{
};

template<typename A, typename B, template<typename, typename> typename C = DefaultPair>
Pair(const A&, const B&) -> Pair<A, B, C>;

template<typename A, typename B, template<typename, typename> typename C = DefaultPair>
constexpr bool
operator==(const Pair<A, B, C>& l, const Pair<A, B, C>& r)
{
    auto& [lFirst, lSecond] = l;
    auto& [rFirst, rSecond] = r;

    return lFirst == rFirst && lSecond == rSecond;
}

template<typename A, typename B, template<typename, typename> typename C = DefaultPair>
constexpr bool
operator!=(const Pair<A, B, C>& l, const Pair<A, B, C>& r)
{
    return !(l == r);
}

template<typename A, typename B, template<typename, typename> typename C = DefaultPair>
constexpr bool
operator<(const Pair<A, B, C>& l, const Pair<A, B, C>& r)
{
    auto& [lFirst, lSecond] = l;
    auto& [rFirst, rSecond] = r;

    return lFirst < rFirst && lSecond < rSecond;
}

template<typename A, typename B, template<typename, typename> typename C = DefaultPair>
constexpr bool
operator>(const Pair<A, B, C>& l, const Pair<A, B, C>& r)
{
    auto& [lFirst, lSecond] = l;
    auto& [rFirst, rSecond] = r;

    return lFirst > rFirst && lSecond > rSecond;
}

template<typename A, typename B, template<typename, typename> typename C = DefaultPair>
constexpr bool
operator<=(const Pair<A, B, C>& l, const Pair<A, B, C>& r)
{
    auto& [lFirst, lSecond] = l;
    auto& [rFirst, rSecond] = r;

    return lFirst <= rFirst && lSecond <= rSecond;
}

template<typename A, typename B, template<typename, typename> typename C = DefaultPair>
constexpr bool
operator>=(const Pair<A, B, C>& l, const Pair<A, B, C>& r)
{
    auto& [lFirst, lSecond] = l;
    auto& [rFirst, rSecond] = r;

    return lFirst >= rFirst && lSecond >= rSecond;
}

namespace print
{

template<typename A, typename B, template<typename, typename> typename C = DefaultPair>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const Pair<A, B, C>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    auto& [first, second] = x;
    return printArgs(ctx, first, second);
}

} /* namespace print */

} /* namespace adt */

#pragma once

#include "types.hh"

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

} /* namespace adt */

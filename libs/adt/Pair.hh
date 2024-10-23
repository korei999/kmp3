#pragma once

namespace adt
{

template<typename A, typename B>
struct Pair
{
    A x {};
    B y {};
};

template<typename A, typename B>
constexpr bool
operator==(const Pair<A, B>& l, const Pair<A, B>& r)
{
    return l.x == r.x && l.y == r.y;
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
    return l.x < r.x && l.y < r.y;
}

template<typename A, typename B>
constexpr bool
operator>(const Pair<A, B>& l, const Pair<A, B>& r)
{
    return l.x > r.x && l.y > r.y;
}

template<typename A, typename B>
constexpr bool
operator<=(const Pair<A, B>& l, const Pair<A, B>& r)
{
    return l.x <= r.x && l.y <= r.y;
}

template<typename A, typename B>
constexpr bool
operator>=(const Pair<A, B>& l, const Pair<A, B>& r)
{
    return l.x >= r.x && l.y >= r.y;
}

} /* namespace adt */

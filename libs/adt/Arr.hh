#pragma once

#include "print.hh"
#include "sort.hh"

#include <cassert>
#include <initializer_list>

namespace adt
{

template<typename T, u32 CAP> requires(CAP > 0) struct Arr;

template<typename T, u32 CAP>
constexpr u32 ArrPush(Arr<T, CAP>* s, const T& x);

template<typename T, u32 CAP>
constexpr u32 ArrFakePush(Arr<T, CAP>* s);

template<typename T, u32 CAP>
constexpr T* ArrPop(Arr<T, CAP>* s);

template<typename T, u32 CAP>
constexpr void ArrFakePop(Arr<T, CAP>* s);

template<typename T, u32 CAP>
constexpr u32 ArrCap([[maybe_unused]] Arr<T, CAP>* s);

template<typename T, u32 CAP>
constexpr u32 ArrSize(Arr<T, CAP>* s);

template<typename T, u32 CAP>
constexpr void ArrSetSize(Arr<T, CAP>* s, u32 newSize);

template<typename T, u32 CAP>
constexpr u32 ArrIdx(Arr<T, CAP>* s, const T* p);

template<typename T, u32 CAP>
constexpr T& ArrFirst(Arr<T, CAP>* s);

template<typename T, u32 CAP>
constexpr const T& ArrFirst(const Arr<T, CAP>* s);

template<typename T, u32 CAP>
constexpr T& ArrLast(Arr<T, CAP>* s);

template<typename T, u32 CAP>
constexpr const T& ArrLast(const Arr<T, CAP>* s);

/* statically sized array */
template<typename T, u32 CAP> requires(CAP > 0)
struct Arr
{
    T aData[CAP] {};
    u32 size {};

    constexpr T& operator[](u32 i) { assert(i < size && "[Arr]: out of size access"); return aData[i]; }
    constexpr const T& operator[](u32 i) const { assert(i < CAP && "[Arr]: out of capacity access"); return aData[i]; }

    constexpr Arr() = default;
    constexpr Arr(std::initializer_list<T> list);

    struct It
    {
        T* s;

        It(T* pFirst) : s{pFirst} {}

        constexpr T& operator*() { return *s; }
        constexpr T* operator->() { return s; }

        constexpr It operator++() { s++; return *this; }
        constexpr It operator++(int) { T* tmp = s++; return tmp; }

        constexpr It operator--() { s--; return *this; }
        constexpr It operator--(int) { T* tmp = s--; return tmp; }

        friend constexpr bool operator==(const It& l, const It& r) { return l.s == r.s; }
        friend constexpr bool operator!=(const It& l, const It& r) { return l.s != r.s; }
    };

    constexpr It begin() { return {&this->aData[0]}; }
    constexpr It end() { return {&this->aData[this->size]}; }
    constexpr It rbegin() { return {&this->aData[this->size - 1]}; }
    constexpr It rend() { return {this->aData - 1}; }

    constexpr const It begin() const { return {&this->aData[0]}; }
    constexpr const It end() const { return {&this->aData[this->size]}; }
    constexpr const It rbegin() const { return {&this->aData[this->size - 1]}; }
    constexpr const It rend() const { return {this->aData - 1}; }
};

template<typename T, u32 CAP>
constexpr u32
ArrPush(Arr<T, CAP>* s, const T& x)
{
    assert(s->size < CAP && "[Arr]: pushing over capacity");

    s->aData[s->size++] = x;
    return s->size - 1;
}

template<typename T, u32 CAP>
constexpr u32
ArrFakePush(Arr<T, CAP>* s)
{
    assert(s->size < CAP && "[Arr]: fake push over capacity");
    ++s->size;
    return s->size - 1;
}

template<typename T, u32 CAP>
constexpr T*
ArrPop(Arr<T, CAP>* s)
{
    assert(s->size > 0 && "[Arr]: pop from empty");
    return &s->aData[--s->size];
}

template<typename T, u32 CAP>
constexpr void
ArrFakePop(Arr<T, CAP>* s)
{
    assert(s->size > 0 && "[Arr]: pop from empty");
    --s->size;
}

template<typename T, u32 CAP>
constexpr u32
ArrCap([[maybe_unused]] Arr<T, CAP>* s)
{
    return CAP;
}

template<typename T, u32 CAP>
constexpr u32
ArrSize(Arr<T, CAP>* s)
{
    return s->size;
}

template<typename T, u32 CAP>
constexpr void
ArrSetSize(Arr<T, CAP>* s, u32 newSize)
{
    assert(newSize <= CAP && "[Arr]: cannot enlarge static array");
    s->size = newSize;
}

template<typename T, u32 CAP>
constexpr u32
ArrIdx(Arr<T, CAP>* s, const T* p)
{
    u32 r = u32(p - s->aData);
    assert(r < s->cap);
    return r;
}

template<typename T, u32 CAP>
constexpr T&
ArrFirst(Arr<T, CAP>* s)
{
    return s->operator[](0);
}

template<typename T, u32 CAP>
constexpr const T&
ArrFirst(const Arr<T, CAP>* s)
{
    return s->operator[](0);
}

template<typename T, u32 CAP>
constexpr T&
ArrLast(Arr<T, CAP>* s)
{
    return s->operator[](s->size - 1);
}

template<typename T, u32 CAP>
constexpr const T&
ArrLast(const Arr<T, CAP>* s)
{
    return s->operator[](s->size - 1);
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr Arr<T, CAP>::Arr(std::initializer_list<T> list)
{
    for (auto& e : list) ArrPush(this, e);
}

namespace utils
{

template<typename T, u32 CAP>
[[nodiscard]] constexpr bool
empty(const Arr<T, CAP>* s)
{
    return s->size == 0;
}

} /* namespace utils */

namespace sort
{

template<typename T, u32 CAP, decltype(utils::compare<T>) FN_CMP = utils::compare>
constexpr void
quick(Arr<T, CAP>* pArr)
{
    if (pArr->size <= 1) return;

    quick<T, FN_CMP>(pArr->aData, 0, pArr->size - 1);
}

template<typename T, u32 CAP, decltype(utils::compare<T>) FN_CMP = utils::compare>
constexpr void
insertion(Arr<T, CAP>* pArr)
{
    if (pArr->size <= 1) return;

    insertion<T, FN_CMP>(pArr->aData, 0, pArr->size - 1);
}

} /* namespace sort */

namespace print
{

template<typename T, u32 CAP>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const Arr<T, CAP>& x)
{
    if (utils::empty(&x))
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "(empty)");
    }

    char aBuff[1024] {};
    u32 nRead = 0;
    for (u32 i = 0; i < x.size; ++i)
    {
        const char* fmt;
        if constexpr (std::is_floating_point_v<T>) fmt = i == x.size - 1 ? "{:.3}" : "{:.3}, ";
        else fmt = i == x.size - 1 ? "{}" : "{}, ";

        nRead += toBuffer(aBuff + nRead, utils::size(aBuff) - nRead, fmt, x[i]);
    }

    return print::copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

} /* namespace print */

} /* namespace adt */

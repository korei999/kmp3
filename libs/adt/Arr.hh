#pragma once

#include "print.hh"
#include "sort.hh"

#include <cassert>

namespace adt
{

/* statically sized array */
template<typename T, u32 CAP>
struct Arr
{
    T pData[CAP] {};
    u32 size {};

    T& operator[](u32 i)             { assert(i < size && "[Arr]: out of size access"); return pData[i]; }
    const T& operator[](u32 i) const { assert(i < CAP && "[Arr]: out of capacity access"); return pData[i]; }

    struct It
    {
        T* s;

        It(T* pFirst) : s{pFirst} {}

        T& operator*() { return *s; }
        T* operator->() { return s; }

        It operator++() { s++; return *this; }
        It operator++(int) { T* tmp = s++; return tmp; }

        It operator--() { s--; return *this; }
        It operator--(int) { T* tmp = s--; return tmp; }

        friend constexpr bool operator==(const It& l, const It& r) { return l.s == r.s; }
        friend constexpr bool operator!=(const It& l, const It& r) { return l.s != r.s; }
    };

    It begin() { return {&this->pData[0]}; }
    It end() { return {&this->pData[this->size]}; }
    It rbegin() { return {&this->pData[this->size - 1]}; }
    It rend() { return {this->pData - 1}; }

    const It begin() const { return {&this->pData[0]}; }
    const It end() const { return {&this->pData[this->size]}; }
    const It rbegin() const { return {&this->pData[this->size - 1]}; }
    const It rend() const { return {this->pData - 1}; }
};

template<typename T, u32 CAP>
inline u32
ArrPush(Arr<T, CAP>* s, const T& x)
{
    assert(CAP > 0 && "[Arr]: ininitialized push");
    assert(s->size < CAP && "[Arr]: pushing over capacity");

    s->pData[s->size++] = x;
    return s->size - 1;
}

template<typename T, u32 CAP>
inline u32
ArrCap(Arr<T, CAP>* s)
{
    return utils::size(s->pData);
}

template<typename T, u32 CAP>
inline u32
ArrSize(Arr<T, CAP>* s)
{
    return s->size;
}

template<typename T, u32 CAP>
inline void
ArrSetSize(Arr<T, CAP>* s, u32 newSize)
{
    assert(newSize <= CAP && "[Arr]: cannot enlarge static array");
    s->size = newSize;
}

template<typename T, u32 CAP>
inline u32
ArrIdx(Arr<T, CAP>* s, const T* p)
{
    u32 r = u32(p - s->pData);
    assert(r < s->cap);
    return r;
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

template<typename T, u32 CAP, auto FN_CMP = utils::compare<T>>
constexpr void
quick(Arr<T, CAP>* pArr)
{
    if (pArr->size <= 1) return;

    quick<T, FN_CMP>(pArr->pData, 0, pArr->size - 1);
}

template<typename T, u32 CAP, auto FN_CMP = utils::compare<T>>
constexpr void
insertion(Arr<T, CAP>* pArr)
{
    if (pArr->size <= 1) return;

    insertion<T, FN_CMP>(pArr->pData, 0, pArr->size - 1);
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

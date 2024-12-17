#pragma once

#include "print.hh"
#include "sort.hh"

#include <cassert>
#include <initializer_list>

namespace adt
{

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

    constexpr u32 push(const T& x);

    constexpr u32 fakePush();

    constexpr T* pop();

    constexpr void fakePop();

    constexpr u32 getCap() const;

    constexpr u32 getSize() const;

    constexpr void setSize(u32 newSize);

    constexpr u32 idx(const T* p);

    constexpr T& first();

    constexpr const T& first() const;

    constexpr T& last();

    constexpr const T& last() const;
};

template<typename T, u32 CAP> requires(CAP > 0)
constexpr u32
Arr<T, CAP>::push(const T& x)
{
    assert(this->getSize() < CAP && "[Arr]: pushing over capacity");

    this->aData[this->size++] = x;
    return this->size - 1;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr u32
Arr<T, CAP>::fakePush()
{
    assert(this->size < CAP && "[Arr]: fake push over capacity");
    ++this->size;
    return this->size - 1;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr T*
Arr<T, CAP>::pop()
{
    assert(this->size > 0 && "[Arr]: pop from empty");
    return &this->aData[--this->size];
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr void
Arr<T, CAP>::fakePop()
{
    assert(this->size > 0 && "[Arr]: pop from empty");
    --this->size;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr u32
Arr<T, CAP>::getCap() const
{
    return CAP;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr u32
Arr<T, CAP>::getSize() const
{
    return this->size;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr void
Arr<T, CAP>::setSize(u32 newSize)
{
    assert(newSize <= CAP && "[Arr]: cannot enlarge static array");
    this->size = newSize;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr u32
Arr<T, CAP>::idx(const T* p)
{
    u32 r = u32(p - this->aData);
    assert(r < this->getCap());
    return r;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr T&
Arr<T, CAP>::first()
{
    return this->operator[](0);
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr const T&
Arr<T, CAP>::first() const
{
    return this->operator[](0);
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr T&
Arr<T, CAP>::last()
{
    return this->operator[](this->size - 1);
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr const T&
Arr<T, CAP>::last() const
{
    return this->operator[](this->size - 1);
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr Arr<T, CAP>::Arr(std::initializer_list<T> list)
{
    for (auto& e : list) this->push(e);
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

#pragma once

#include "print.hh"
#include "sort.hh"

#include <cassert>
#include <initializer_list>
#include <new> /* IWYU pragma: keep */

namespace adt
{

/* statically sized array */
template<typename T, u32 CAP> requires(CAP > 0)
struct Arr
{
    T m_aData[CAP] {};
    u32 m_size {};

    /* */

    constexpr Arr() = default;
    constexpr Arr(std::initializer_list<T> list);

    /* */

    constexpr T& operator[](u32 i) { assert(i < m_size && "[Arr]: out of size access"); return m_aData[i]; }
    constexpr const T& operator[](u32 i) const { assert(i < CAP && "[Arr]: out of capacity access"); return m_aData[i]; }

    /* */

    constexpr T* data() { return m_aData; }
    constexpr const T* data() const { return m_aData; }
    constexpr bool empty() const { return m_size == 0; }
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

    /* */

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

    constexpr It begin() { return {&m_aData[0]}; }
    constexpr It end() { return {&m_aData[m_size]}; }
    constexpr It rbegin() { return {&m_aData[m_size - 1]}; }
    constexpr It rend() { return {m_aData - 1}; }

    constexpr const It begin() const { return {&m_aData[0]}; }
    constexpr const It end() const { return {&m_aData[m_size]}; }
    constexpr const It rbegin() const { return {&m_aData[m_size - 1]}; }
    constexpr const It rend() const { return {m_aData - 1}; }
};

template<typename T, u32 CAP> requires(CAP > 0)
constexpr u32
Arr<T, CAP>::push(const T& x)
{
    assert(getSize() < CAP && "[Arr]: pushing over capacity");

    new(m_aData + m_size++) T(x);

    return m_size - 1;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr u32
Arr<T, CAP>::fakePush()
{
    assert(m_size < CAP && "[Arr]: fake push over capacity");
    ++m_size;
    return m_size - 1;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr T*
Arr<T, CAP>::pop()
{
    assert(m_size > 0 && "[Arr]: pop from empty");
    return &m_aData[--m_size];
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr void
Arr<T, CAP>::fakePop()
{
    assert(m_size > 0 && "[Arr]: pop from empty");
    --m_size;
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
    return m_size;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr void
Arr<T, CAP>::setSize(u32 newSize)
{
    assert(newSize <= CAP && "[Arr]: cannot enlarge static array");
    m_size = newSize;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr u32
Arr<T, CAP>::idx(const T* p)
{
    u32 r = u32(p - m_aData);
    assert(r < getCap());
    return r;
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr T&
Arr<T, CAP>::first()
{
    return operator[](0);
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr const T&
Arr<T, CAP>::first() const
{
    return operator[](0);
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr T&
Arr<T, CAP>::last()
{
    return operator[](m_size - 1);
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr const T&
Arr<T, CAP>::last() const
{
    return operator[](m_size - 1);
}

template<typename T, u32 CAP> requires(CAP > 0)
constexpr Arr<T, CAP>::Arr(std::initializer_list<T> list)
{
    for (auto& e : list) push(e);
}

namespace sort
{

template<typename T, u32 CAP, decltype(utils::compare<T>) FN_CMP = utils::compare>
constexpr void
quick(Arr<T, CAP>* pArr)
{
    if (pArr->m_size <= 1) return;

    quick<T, FN_CMP>(pArr->m_aData, 0, pArr->m_size - 1);
}

template<typename T, u32 CAP, decltype(utils::compare<T>) FN_CMP = utils::compare>
constexpr void
insertion(Arr<T, CAP>* pArr)
{
    if (pArr->m_size <= 1) return;

    insertion<T, FN_CMP>(pArr->m_aData, 0, pArr->m_size - 1);
}

} /* namespace sort */

namespace print
{

template<typename T, u32 CAP>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const Arr<T, CAP>& x)
{
    if (x.empty())
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "(empty)");
    }

    char aBuff[1024] {};
    u32 nRead = 0;
    for (u32 i = 0; i < x.m_size; ++i)
    {
        const char* fmt = i == x.m_size - 1 ? "{}" : "{}, ";
        nRead += toBuffer(aBuff + nRead, utils::size(aBuff) - nRead, fmt, x[i]);
    }

    return print::copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

} /* namespace print */

} /* namespace adt */

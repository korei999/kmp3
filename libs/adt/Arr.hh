#pragma once

#include "print.hh"
#include "sort.hh"

#include <cassert>
#include <initializer_list>
#include <new> /* IWYU pragma: keep */
#include <utility>

namespace adt
{

/* statically sized array */
template<typename T, ssize CAP> requires(CAP > 0)
struct Arr
{
    T m_aData[CAP] {};
    ssize m_size {};

    /* */

    constexpr Arr() = default;
    constexpr Arr(std::initializer_list<T> list);

    /* */

    constexpr T& operator[](ssize i) { assert(i < m_size && "[Arr]: out of size access"); return m_aData[i]; }
    constexpr const T& operator[](ssize i) const { assert(i < CAP && "[Arr]: out of capacity access"); return m_aData[i]; }

    /* */

    constexpr T* data() { return m_aData; }
    constexpr const T* data() const { return m_aData; }
    constexpr bool empty() const { return m_size == 0; }
    constexpr ssize push(const T& x);
    template<typename ...ARGS> requires (std::is_constructible_v<T, ARGS...>) constexpr ssize emplace(ARGS&&... args);
    constexpr ssize fakePush();
    constexpr T* pop();
    constexpr void fakePop();
    constexpr ssize getCap() const;
    constexpr ssize getSize() const;
    constexpr void setSize(ssize newSize);
    constexpr ssize idx(const T* p);
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

template<typename T, ssize CAP> requires(CAP > 0)
constexpr ssize
Arr<T, CAP>::push(const T& x)
{
    assert(getSize() < CAP && "[Arr]: pushing over capacity");

    new(m_aData + m_size++) T(x);

    return m_size - 1;
}

template<typename T, ssize CAP> requires(CAP > 0)
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline constexpr ssize
Arr<T, CAP>::emplace(ARGS&&... args)
{
    assert(getSize() < CAP && "[Arr]: pushing over capacity");

    new(m_aData + m_size++) T(std::forward<ARGS>(args)...);

    return m_size - 1;
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr ssize
Arr<T, CAP>::fakePush()
{
    assert(m_size < CAP && "[Arr]: fake push over capacity");
    ++m_size;
    return m_size - 1;
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr T*
Arr<T, CAP>::pop()
{
    assert(m_size > 0 && "[Arr]: pop from empty");
    return &m_aData[--m_size];
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr void
Arr<T, CAP>::fakePop()
{
    assert(m_size > 0 && "[Arr]: pop from empty");
    --m_size;
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr ssize
Arr<T, CAP>::getCap() const
{
    return CAP;
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr ssize
Arr<T, CAP>::getSize() const
{
    return m_size;
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr void
Arr<T, CAP>::setSize(ssize newSize)
{
    assert(newSize <= CAP && "[Arr]: cannot enlarge static array");
    m_size = newSize;
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr ssize
Arr<T, CAP>::idx(const T* p)
{
    ssize r = ssize(p - m_aData);
    assert(r < getCap());
    return r;
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr T&
Arr<T, CAP>::first()
{
    return operator[](0);
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr const T&
Arr<T, CAP>::first() const
{
    return operator[](0);
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr T&
Arr<T, CAP>::last()
{
    return operator[](m_size - 1);
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr const T&
Arr<T, CAP>::last() const
{
    return operator[](m_size - 1);
}

template<typename T, ssize CAP> requires(CAP > 0)
constexpr Arr<T, CAP>::Arr(std::initializer_list<T> list)
{
    for (auto& e : list) push(e);
}

namespace sort
{

template<typename T, ssize CAP, auto FN_CMP = utils::compare<T>>
constexpr void
quick(Arr<T, CAP>* pArr)
{
    if (pArr->m_size <= 1) return;

    quick<T, FN_CMP>(pArr->m_aData, 0, pArr->m_size - 1);
}

template<typename T, ssize CAP, auto FN_CMP = utils::compare<T>>
constexpr void
insertion(Arr<T, CAP>* pArr)
{
    if (pArr->m_size <= 1) return;

    insertion<T, FN_CMP>(pArr->m_aData, 0, pArr->m_size - 1);
}

} /* namespace sort */

namespace print
{

template<typename T, ssize CAP>
inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const Arr<T, CAP>& x)
{
    if (x.empty())
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "(empty)");
    }

    char aBuff[1024] {};
    ssize nRead = 0;
    for (ssize i = 0; i < x.m_size; ++i)
    {
        const char* fmt = i == x.m_size - 1 ? "{}" : "{}, ";
        nRead += toBuffer(aBuff + nRead, utils::size(aBuff) - nRead, fmt, x[i]);
    }

    return print::copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

} /* namespace print */

} /* namespace adt */

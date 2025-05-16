#pragma once

#include "print.hh"
#include "sort.hh"

#include <initializer_list>
#include <new>

namespace adt
{

/* statically sized array */
template<typename T, isize CAP> requires(CAP > 0)
struct Array
{
    T m_aData[CAP] {};
    isize m_size {};

    /* */

    constexpr Array() = default;

    constexpr Array(isize size);

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    constexpr Array(isize size, ARGS&&... args);

    constexpr Array(std::initializer_list<T> list);

    /* */

    constexpr operator Span<T>() const { return {data(), size()}; }

    /* */

#define ADT_RANGE_CHECK ADT_ASSERT(i >= 0 && i < m_size, "i: {}, m_size: {}", i, m_size);

    constexpr T& operator[](isize i)             { ADT_RANGE_CHECK; return m_aData[i]; }
    constexpr const T& operator[](isize i) const { ADT_RANGE_CHECK; return m_aData[i]; }

#undef ADT_RANGE_CHECK

    /* */

    constexpr T* data() { return m_aData; }
    constexpr const T* data() const { return m_aData; }
    constexpr bool empty() const { return m_size <= 0; }
    isize push(const T& x); /* placement new cannot be constexpr something... */
    template<typename ...ARGS> requires (std::is_constructible_v<T, ARGS...>) constexpr isize emplace(ARGS&&... args);
    constexpr isize fakePush();
    constexpr T* pop();
    constexpr void fakePop();
    constexpr isize cap() const;
    constexpr isize size() const;
    constexpr void setSize(isize newSize);
    constexpr isize idx(const T* const p) const;
    constexpr T& first();
    constexpr const T& first() const;
    constexpr T& last();
    constexpr const T& last() const;

    /* */

    struct It
    {
        T* s;

        constexpr It(const T* pFirst) : s{const_cast<T*>(pFirst)} {}

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

template<typename T, isize CAP> requires(CAP > 0)
inline isize
Array<T, CAP>::push(const T& x)
{
    ADT_ASSERT(size() < CAP, "pushing over capacity");

    new(m_aData + m_size++) T(x);

    return m_size - 1;
}

template<typename T, isize CAP> requires(CAP > 0)
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline constexpr isize
Array<T, CAP>::emplace(ARGS&&... args)
{
    ADT_ASSERT(size() < CAP, "pushing over capacity");

    new(m_aData + m_size++) T(forward<ARGS>(args)...);

    return m_size - 1;
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr isize
Array<T, CAP>::fakePush()
{
    ADT_ASSERT(m_size < CAP, "push over capacity");
    ++m_size;
    return m_size - 1;
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr T*
Array<T, CAP>::pop()
{
    ADT_ASSERT(m_size > 0, "empty");
    return &m_aData[--m_size];
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr void
Array<T, CAP>::fakePop()
{
    ADT_ASSERT(m_size > 0, "empty");
    --m_size;
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr isize
Array<T, CAP>::cap() const
{
    return CAP;
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr isize
Array<T, CAP>::size() const
{
    return m_size;
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr void
Array<T, CAP>::setSize(isize newSize)
{
    ADT_ASSERT(newSize <= CAP, "cannot enlarge static array");
    m_size = newSize;
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr isize
Array<T, CAP>::idx(const T* const p) const
{
    isize r = isize(p - m_aData);
    ADT_ASSERT(r >= 0 && r < size(), "out of range, r: {}, size: {}", r, size());
    return r;
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr T&
Array<T, CAP>::first()
{
    return operator[](0);
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr const T&
Array<T, CAP>::first() const
{
    return operator[](0);
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr T&
Array<T, CAP>::last()
{
    return operator[](m_size - 1);
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr const T&
Array<T, CAP>::last() const
{
    return operator[](m_size - 1);
}

template<typename T, isize CAP> requires(CAP > 0)
inline constexpr
Array<T, CAP>::Array(isize size) : m_size(size) {}

template<typename T, isize CAP> requires(CAP > 0)
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline constexpr
Array<T, CAP>::Array(isize size, ARGS&&... args)
    : m_size(size)
{
    ADT_ASSERT(size <= CAP, " ");

    for (isize i = 0; i < size; ++i)
        new(m_aData + i) T(std::forward<ARGS>(args)...);
}

template<typename T, isize CAP> requires(CAP > 0)
constexpr Array<T, CAP>::Array(std::initializer_list<T> list)
{
    setSize(list.size());
    for (isize i = 0; i < size(); ++i)
        m_aData[i] = *(list.begin() + i);
}

namespace sort
{

template<typename T, isize CAP, auto FN_CMP = utils::compare<T>>
constexpr void
quick(Array<T, CAP>* pArr)
{
    if (pArr->m_size <= 1) return;

    quick<T, FN_CMP>(pArr->m_aData, 0, pArr->m_size - 1);
}

template<typename T, isize CAP, auto FN_CMP = utils::compare<T>>
constexpr void
insertion(Array<T, CAP>* pArr)
{
    if (pArr->m_size <= 1) return;

    insertion<T, FN_CMP>(pArr->m_aData, 0, pArr->m_size - 1);
}

} /* namespace sort */

namespace print
{

/* adapt to CON_T<T> template */
template<typename T, isize CAP>
inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, const Array<T, CAP>& x)
{
    return print::formatToContext(ctx, fmtArgs, Span(x.data(), x.size()));
}

} /* namespace print */

} /* namespace adt */

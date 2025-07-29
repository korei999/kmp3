#pragma once

#include "print.hh"

#include <initializer_list>
#include <new> /* IWYU pragma: keep */

namespace adt
{

/* statically sized array */
template<typename T, isize CAP>
struct Array
{
    static_assert(CAP > 0);

    T m_aData[CAP];
    isize m_size {};

    /* */

    constexpr Array() = default;

    constexpr Array(isize size);

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    constexpr Array(isize size, ARGS&&... args);

    constexpr Array(const std::initializer_list<T> list);

    /* */

    constexpr operator Span<T>() { return {m_aData, m_size}; }
    constexpr operator const Span<const T>() const { return {m_aData, m_size}; }

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
    isize push(T&& x);

    void pushAt(isize i, const T& x);
    void pushAt(isize i, T&& x);

    template<typename ...ARGS> requires (std::is_constructible_v<T, ARGS...>)
    void emplaceAt(isize i, ARGS&&... x);

    template<typename ...ARGS> requires (std::is_constructible_v<T, ARGS...>)
    constexpr isize emplace(ARGS&&... args);

    constexpr isize fakePush();

    constexpr T& pop() noexcept;

    constexpr void fakePop() noexcept;

    constexpr isize cap() const noexcept;

    constexpr isize size() const noexcept;

    constexpr void setSize(isize newSize);

    constexpr isize idx(const T* const p) const noexcept;

    constexpr T& first() noexcept;
    constexpr const T& first() const noexcept;

    constexpr T& last() noexcept;
    constexpr const T& last() const noexcept;

    /* */

    constexpr T* begin() noexcept { return {&m_aData[0]}; }
    constexpr T* end() noexcept { return {&m_aData[m_size]}; }
    constexpr T* rbegin() noexcept { return {&m_aData[m_size - 1]}; }
    constexpr T* rend() noexcept { return {m_aData - 1}; }

    constexpr const T* begin() const noexcept { return {&m_aData[0]}; }
    constexpr const T* end() const noexcept { return {&m_aData[m_size]}; }
    constexpr const T* rbegin() const noexcept { return {&m_aData[m_size - 1]}; }
    constexpr const T* rend() const noexcept { return {m_aData - 1}; }
};

template<typename T, isize CAP>
inline isize
Array<T, CAP>::push(const T& x)
{
    ADT_ASSERT(size() < CAP, "pushing over capacity");

    if constexpr (!std::is_trivially_destructible_v<T>)
        m_aData[m_size].~T();

    new(m_aData + m_size++) T(x);

    return m_size - 1;
}

template<typename T, isize CAP>
inline isize
Array<T, CAP>::push(T&& x)
{
    return emplace(std::move(x));
}

template<typename T, isize CAP>
inline void
Array<T, CAP>::pushAt(const isize i, const T& x)
{
    emplaceAt(i, x);
}

template<typename T, isize CAP>
inline void
Array<T, CAP>::pushAt(const isize i, T&& x)
{
    emplaceAt(i, std::move(x));
}

template<typename T, isize CAP>
template<typename ...ARGS> requires (std::is_constructible_v<T, ARGS...>)
inline void
Array<T, CAP>::emplaceAt(isize i, ARGS&&... x)
{
    fakePush();
    utils::memMove(&operator[](i + 1), &operator[](i), size() - 1 - i);
    new(&operator[](i)) T {std::forward<ARGS>(x)...};
}

template<typename T, isize CAP>
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline constexpr isize
Array<T, CAP>::emplace(ARGS&&... args)
{
    ADT_ASSERT(size() < CAP, "pushing over capacity");

    if constexpr (!std::is_trivially_destructible_v<T>)
        m_aData[m_size].~T();

    new(m_aData + m_size++) T(std::forward<ARGS>(args)...);

    return m_size - 1;
}

template<typename T, isize CAP>
constexpr isize
Array<T, CAP>::fakePush()
{
    ADT_ASSERT(m_size < CAP, "push over capacity");

    if constexpr (!std::is_trivially_destructible_v<T>)
        m_aData[m_size].~T();

    ++m_size;
    return m_size - 1;
}

template<typename T, isize CAP>
constexpr T&
Array<T, CAP>::pop() noexcept
{
    ADT_ASSERT(m_size > 0, "empty");
    return m_aData[--m_size];
}

template<typename T, isize CAP>
constexpr void
Array<T, CAP>::fakePop() noexcept
{
    ADT_ASSERT(m_size > 0, "empty");
    --m_size;
}

template<typename T, isize CAP>
constexpr isize
Array<T, CAP>::cap() const noexcept
{
    return CAP;
}

template<typename T, isize CAP>
constexpr isize
Array<T, CAP>::size() const noexcept
{
    return m_size;
}

template<typename T, isize CAP>
constexpr void
Array<T, CAP>::setSize(isize newSize)
{
    ADT_ASSERT(newSize <= CAP, "cannot enlarge static array");
    m_size = newSize;
}

template<typename T, isize CAP>
constexpr isize
Array<T, CAP>::idx(const T* const p) const noexcept
{
    isize r = isize(p - m_aData);
    ADT_ASSERT(r >= 0 && r < size(), "out of range, r: {}, size: {}", r, size());
    return r;
}

template<typename T, isize CAP>
constexpr T&
Array<T, CAP>::first() noexcept
{
    return operator[](0);
}

template<typename T, isize CAP>
constexpr const T&
Array<T, CAP>::first() const noexcept
{
    return operator[](0);
}

template<typename T, isize CAP>
constexpr T&
Array<T, CAP>::last() noexcept
{
    return operator[](m_size - 1);
}

template<typename T, isize CAP>
constexpr const T&
Array<T, CAP>::last() const noexcept
{
    return operator[](m_size - 1);
}

template<typename T, isize CAP>
inline constexpr
Array<T, CAP>::Array(isize size) : m_aData {}, m_size(size) {}

template<typename T, isize CAP>
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline constexpr
Array<T, CAP>::Array(isize size, ARGS&&... args)
    : m_size(size)
{
    ADT_ASSERT(size <= CAP, " ");

    for (isize i = 0; i < size; ++i)
        m_aData[i] = T {std::forward<ARGS>(args)...};
}

template<typename T, isize CAP>
constexpr Array<T, CAP>::Array(const std::initializer_list<T> list)
{
    setSize(list.size());
    for (isize i = 0; i < size(); ++i)
        m_aData[i] = list.begin()[i];
}

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

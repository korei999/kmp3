#pragma once

#include "types.hh"
#include "assert.hh"

namespace adt
{

/* Span with byteStride between elements, byteStride of Zero means tightly packed. */
template<typename T>
struct View
{
    T* m_pData {};
    isize m_count {};
    isize m_byteStride {};

    /* */

    View() = default;
    View(const T* pData, isize size, isize stride);

    /* */

    T& operator[](isize i) { return at(i); }
    const T& operator[](isize i) const { return at(i); }

    bool empty() const { return m_count <= 0; }
    isize size() const { return m_count; }  /* NOTE: element count (not byte size). */
    isize stride() const { return m_byteStride; }
    isize idx(const T* const pElement) const; /* NOTE: requires division (slow), for i loops should be preferred instead. */

protected:
    T& at(isize i) const;
    u8* u8Data() { return (u8*)(m_pData); }
    const u8* u8Data() const { return (const u8*)(m_pData); }

    /* */
public:

    struct It
    {
        View* pView;
        isize i {};

        /* */

        It(const View* self, isize _i) : pView(const_cast<View*>(self)), i(_i) {}

        /* */

        T& operator*() noexcept { return pView->operator[](i); }
        T* operator->() noexcept { return &pView->operator[](i); }

        It operator++() noexcept { ++i; return *this; }

        It operator--() noexcept { --i; return *this; }

        friend constexpr bool operator==(const It& l, const It& r) noexcept { return l.i == r.i; }
        friend constexpr bool operator!=(const It& l, const It& r) noexcept { return l.i != r.i; }
    };

    It begin()  noexcept { return {this, 0}; }
    It end()    noexcept { return {this, m_count}; }
    It rbegin() noexcept { return {this, m_count - 1}; }
    It rend()   noexcept { return {this, NPOS}; }

    const It begin()  const noexcept { return {this, 0}; }
    const It end()    const noexcept { return {this, m_count}; }
    const It rbegin() const noexcept { return {this, m_count - 1}; }
    const It rend()   const noexcept { return {this, NPOS}; }
};

template<typename T>
View<T>::View(const T* pData, isize size, isize stride)
    : m_pData(const_cast<T*>(pData)), m_count(size)
{
    if (stride == 0) m_byteStride = (sizeof(T));
    else m_byteStride = stride;
}

template<typename T>
inline T&
View<T>::at(isize i) const
{
    ADT_ASSERT(i >= 0 && i < m_count, "i: {}, size: {}, stride: {}", i, m_count, m_byteStride);

    auto* pU8 = u8Data();
    T* pRet = (T*)(pU8 + (i*m_byteStride));
    return *pRet;
}

template<typename T>
inline isize
View<T>::idx(const T* const pElement) const
{
    auto* p = reinterpret_cast<const u8*>(pElement);
    pdiff absIdx = p - u8Data();
    return absIdx / m_byteStride;
}

} /* namespace adt */

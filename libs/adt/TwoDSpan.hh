#pragma once

#include "types.hh"

#include <cassert>

namespace adt
{

template<typename T>
struct TwoDSpan
{
    T* m_pData {};
    ssize m_width {};
    ssize m_height {};

    /* */

    constexpr TwoDSpan() = default;
    constexpr TwoDSpan(T* pData, ssize width, ssize height)
        : m_pData(pData), m_width(width), m_height(height) {}

    /* */

    constexpr T& operator[](ssize x, ssize y) { return at(x, y); }
    constexpr const T& operator[](ssize x, ssize y) const { return at(x, y); }

    constexpr T* data() { return m_pData; }
    constexpr const T* data() const { return m_pData; }

    constexpr ssize getWidth() const { return m_width; }
    constexpr ssize getHeight() const { return m_height; }

    /* */

private:
    constexpr T&
    at(ssize x, ssize y)
    {
        ssize idx = y*m_width + x;
        assert(x < m_width && y < m_height && "[TwoDSpan]: out of range");
        return m_pData[idx];
    }
};

} /* namespace adt */

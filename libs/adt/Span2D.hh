#pragma once

#include "types.hh"

#include <cassert>

namespace adt
{

template<typename T>
struct Span2D
{
    T* m_pData {};
    ssize m_width {};
    ssize m_height {};

    /* */

    constexpr Span2D() = default;
    constexpr Span2D(T* pData, ssize width, ssize height)
        : m_pData(pData), m_width(width), m_height(height) {}

    /* */

    constexpr T& operator()(ssize x, ssize y) { return at(x, y); }
    constexpr const T& operator()(ssize x, ssize y) const { return at(x, y); }

    constexpr operator bool() const { return m_pData != nullptr; }

    constexpr T* data() { return m_pData; }
    constexpr const T* data() const { return m_pData; }

    constexpr ssize getWidth() const { return m_width; }
    constexpr ssize getHeight() const { return m_height; }

    /* */

private:
    constexpr T&
    at(ssize x, ssize y) const
    {
        ssize idx = y*m_width + x;
        ADT_ASSERT(x >= 0 && x < m_width && y >= 0 && y < m_height,
            "x: %lld, y: %lld, width: %lld, height: %lld",
            x, y, m_width, m_height
        );
        return m_pData[idx];
    }
};

} /* namespace adt */

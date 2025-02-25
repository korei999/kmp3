#pragma once

#include "types.hh"

#include <cassert>

namespace adt
{

template<typename T>
struct Span2D
{
    T* m_pData {};
    int m_width {};
    int m_height {};
    int m_stride {};

    /* */

    constexpr Span2D() = default;
    constexpr Span2D(T* pData, int width, int height, int stride)
        : m_pData(pData), m_width(width), m_height(height), m_stride(stride) {}

    /* */

    constexpr T& operator()(int x, int y) { return at(x, y); }
    constexpr const T& operator()(int x, int y) const { return at(x, y); }

    constexpr operator bool() const { return m_pData != nullptr; }

    constexpr T* data() { return m_pData; }
    constexpr const T* data() const { return m_pData; }

    constexpr int width() const { return m_width; }
    constexpr int height() const { return m_height; }
    constexpr int stride() const { return m_stride; }

    /* */

private:
    constexpr T&
    at(int x, int y) const
    {
        ADT_ASSERT(x >= 0 && x < m_width && y >= 0 && y < m_height,
            "x: %d, y: %d, width: %d, height: %d, stride: %d",
            x, y, m_width, m_height, m_stride
        );

        return m_pData[y*m_stride + x];
    }
};

} /* namespace adt */

#pragma once

#include "print.hh" /* IWYU pragma: keep */

namespace adt
{

template<typename T>
struct Span2D
{
    T* m_pData {};
    i32 m_width {};
    i32 m_height {};
    i32 m_stride {};

    /* */

    constexpr Span2D() = default;
    constexpr Span2D(T* pData, i32 width, i32 height, i32 stride)
        : m_pData(pData), m_width(width), m_height(height), m_stride(stride) {}

    /* */

    constexpr operator bool() const { return m_pData != nullptr; }

    /* */

    constexpr T& operator()(i32 x, i32 y) { return at(x, y); }
    constexpr const T& operator()(i32 x, i32 y) const { return at(x, y); }

    constexpr T* data() { return m_pData; }
    constexpr const T* data() const { return m_pData; }

    constexpr i32 width() const { return m_width; }
    constexpr i32 height() const { return m_height; }
    constexpr i32 stride() const { return m_stride; }
    constexpr bool empty() const { return m_width <= 0 || m_height <= 0 || m_stride <= 0; }

    constexpr bool inRange(i32 x, i32 y) const { if (x >= 0 && x < m_width && y >= 0 && y < m_height) return true; else return false; }

    constexpr bool
    tryAt(i32 x, i32 y, auto clFunc)
    {
        if (x >= 0 && x < m_width && y >= 0 && y < m_height)
        {
            clFunc(m_pData[y*m_stride + x]);
            return true;
        }
        else
        {
            return false;
        }
    }

    /* */

protected:
    constexpr T&
    at(i32 x, i32 y)
    {
        ADT_ASSERT(x >= 0 && x < m_width && y >= 0 && y < m_height,
            "x: {}, y: {}, width: {}, height: {}, stride: {}",
            x, y, m_width, m_height, m_stride
        );

        return m_pData[y*m_stride + x];
    }

    constexpr const T&
    at(i32 x, i32 y) const
    {
        ADT_ASSERT(x >= 0 && x < m_width && y >= 0 && y < m_height,
            "x: {}, y: {}, width: {}, height: {}, stride: {}",
            x, y, m_width, m_height, m_stride
        );

        return m_pData[y*m_stride + x];
    }
};

} /* namespace adt */

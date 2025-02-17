#pragma once

#include "adt/print.hh"
#include "adt/utils.hh"
#include "adt/types.hh"

#include <cassert>
#include <cmath>
#include <concepts>
#include <limits>

#if defined __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-braces"
#endif

#if defined __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-braces"
#endif

namespace adt::math
{

constexpr f64 PI64 = 3.14159265358979323846;
constexpr f32 PI32 = static_cast<f32>(PI64);
constexpr f64 EPS64 = std::numeric_limits<f64>::epsilon();
constexpr f32 EPS32 = std::numeric_limits<f32>::epsilon();

constexpr inline f64 toDeg(f64 x) { return x * 180.0 / PI64; }
constexpr inline f64 toRad(f64 x) { return x * PI64 / 180.0; }
constexpr inline f32 toDeg(f32 x) { return x * 180.0f / PI32; }
constexpr inline f32 toRad(f32 x) { return x * PI32 / 180.0f; }

constexpr inline f64 toRad(i64 x) { return toRad(static_cast<f64>(x)); }
constexpr inline f64 toDeg(i64 x) { return toDeg(static_cast<f64>(x)); }
constexpr inline f32 toRad(i32 x) { return toRad(static_cast<f32>(x)); }
constexpr inline f32 toDeg(i32 x) { return toDeg(static_cast<f32>(x)); }

template<typename T>
constexpr inline bool
eq(const T l, const T r)
{
    return l == r;
}

template<>
inline bool
eq(const f64 l, const f64 r)
{
    return std::abs(l - r) <= EPS64*(std::abs(l) + std::abs(r) + 1.0);
}

template<>
inline bool
eq(const f32 l, const f32 r)
{
    return std::abs(l - r) <= EPS32*(std::abs(l) + std::abs(r) + 1.0f);
}

constexpr inline auto sq(const auto& x) { return x * x; }
constexpr inline auto cube(const auto& x) { return x*x*x; }

constexpr inline i64
sign(i64 x)
{
    return (x > 0) - (x < 0);
}

union IV2;

union V2
{
    f32 e[2];
    struct { f32 x, y; };
    struct { f32 u, v; };

    constexpr explicit operator IV2() const;
};

union IV2
{
    int e[2];
    struct { int x, y; };
    struct { int u, v; };

    constexpr explicit operator V2() const
    {
        return {
            static_cast<f32>(x),
            static_cast<f32>(y),
        };
    }
};

constexpr inline
V2::operator IV2() const
{
    return {
        static_cast<int>(x),
        static_cast<int>(y),
    };
}

union V3
{
    f32 e[3];
    struct { V2 xy; f32 _v2pad; };
    struct { f32 x, y, z; };
    struct { f32 r, g, b; };
};

union IV3
{
    int e[3];
    struct { IV2 xy; int _v2pad; };
    struct { int x, y, z; };
    struct { int r, g, b; };
};

union IV4;

union V4
{
    f32 e[4];
    struct { V3 xyz; f32 _v3pad; };
    struct { V2 xy; V2 zw; };
    struct { f32 x, y, z, w; };
    struct { f32 r, g, b, a; };

    /* */

    constexpr explicit operator IV4() const;
};

union IV4
{
    i32 e[4];
    struct { IV3 xyz; i32 _v3pad; };
    struct { IV2 xy; IV2 zw; };
    struct { i32 x, y, z, w; };
    struct { i32 r, g, b, a; };

    /* */

    constexpr explicit operator V4() const
    {
        return {
            static_cast<f32>(x),
            static_cast<f32>(y),
            static_cast<f32>(z),
            static_cast<f32>(w),
        };
    }
};

union IV4u16
{
    u16 e[4];
    struct { u16 x, y, z, w; };

    /* */

    constexpr explicit operator IV4() const
    {
        return {
            static_cast<i32>(x),
            static_cast<i32>(y),
            static_cast<i32>(z),
            static_cast<i32>(w)
        };
    }

    constexpr explicit operator V4() const
    {
        return {
            static_cast<f32>(x),
            static_cast<f32>(y),
            static_cast<f32>(z),
            static_cast<f32>(w)
        };
    }
};

constexpr inline
V4::operator IV4() const
{
    return {
        static_cast<i32>(x),
        static_cast<i32>(y),
        static_cast<i32>(z),
        static_cast<i32>(w),
    };
}

union M2
{
    f32 d[4];
    f32 e[2][2];
    V2 v[2];
};

union M4;

union M3
{
    f32 d[9];
    f32 e[3][3];
    V3 v[3];

    /* */

    constexpr explicit operator M4() const;
};

union M4
{
    f32 d[16];
    f32 e[4][4];
    V4 v[4];

    constexpr explicit operator M3() const
    {
        return {
            e[0][0], e[0][1], e[0][2],
            e[1][0], e[1][1], e[1][2],
            e[2][0], e[2][1], e[2][2]
        };
    };
};

union Qt
{
    V4 base;
    f32 e[4];
    struct { f32 x, y, z, w; };
};

constexpr
M3::operator M4() const
{
    return {
        e[0][0], e[0][1], e[0][2], 0,
        e[1][0], e[1][1], e[1][2], 0,
        e[2][0], e[2][1], e[2][2], 0,
        0,       0,       0,       0
    };
}

constexpr V2
V2From(const f32 x, const f32 y)
{
    return {x, y};
}

constexpr V3
V3From(V2 xy, f32 z)
{
    return {xy.x, xy.y, z};
}

constexpr V3
V3From(f32 x, V2 yz)
{
    return {x, yz.x, yz.y};
}

constexpr V3
V3From(f32 x, f32 y, f32 z)
{
    return {x, y, z};
}

constexpr V4
V4From(const V4& v)
{
    return v;
}

constexpr V4
V4From(const V3& xyz, f32 w)
{
    V4 res; res.xyz = xyz; res.w = w;
    return res;
}

constexpr V4
V4From(const V2& xy, const V2& zw)
{
    V4 res; res.xy = xy; res.zw = zw;
    return res;
}

constexpr V4
V4From(f32 x, const V3& yzw)
{
    V4 res; res.x = x; res.y = yzw.x; res.z = yzw.y; res.w = yzw.z;
    return res;
}

constexpr V4
V4From(f32 x, f32 y, f32 z, f32 w)
{
    return {
        x, y, z, w
    };
}

inline IV2
IV2_F24_8(const V2 v)
{
    return {
        .x = static_cast<i32>(std::round(v.x * 256.0f)),
        .y = static_cast<i32>(std::round(v.y * 256.0f)),
    };
}

inline V2
operator-(const V2& s)
{
    return {.x = -s.x, .y = -s.y};
}

inline V2
operator+(const V2& l, const V2& r)
{
    return {
        .x = l.x + r.x,
        .y = l.y + r.y
    };
}

inline V2
operator-(const V2& l, const V2& r)
{
    return {
        .x = l.x - r.x,
        .y = l.y - r.y
    };
}

inline IV2
operator-(const IV2& l, const IV2& r)
{
    return {
        .x = l.x - r.x,
        .y = l.y - r.y
    };
}

inline V2
operator*(const V2& v, f32 s)
{
    return {
        .x = v.x * s,
        .y = v.y * s
    };
}

inline V2
operator*(f32 s, const V2& v)
{
    return {
        .x = v.x * s,
        .y = v.y * s
    };
}

inline V2
operator*(const V2& l, const V2& r)
{
    return {
        l.x * r.x,
        l.y * r.y
    };
}

inline V2&
operator*=(V2& l, const V2& r)
{
    return l = l * r;
}

inline V2
operator/(const V2& v, f32 s)
{
    return {
        .x = v.x / s,
        .y = v.y / s
    };
}

inline V2&
operator+=(V2& l, const V2& r)
{
    return l = l + r;
}

inline V2&
operator-=(V2& l, const V2& r)
{
    return l = l - r;
}

inline V2&
operator*=(V2& l, f32 r)
{
    return l = l * r;
}

inline V2&
operator/=(V2& l, f32 r)
{
    return l = l / r;
}

inline V3
operator+(const V3& l, const V3& r)
{
    return {
        .x = l.x + r.x,
        .y = l.y + r.y,
        .z = l.z + r.z
    };
}

inline V3
operator-(const V3& l, const V3& r)
{
    return {
        .x = l.x - r.x,
        .y = l.y - r.y,
        .z = l.z - r.z
    };
}

inline V3
operator-(const V3& v)
{
    return {
        -v.x, -v.y, -v.z
    };
}

inline V3
operator*(const V3& v, f32 s)
{
    return {
        .x = v.x * s,
        .y = v.y * s,
        .z = v.z * s
    };
}

inline V3
operator*(f32 s, const V3& v)
{
    return v * s;
}

inline V3
operator*(const V3& l, const V3& r)
{
    return {
        l.x * r.x,
        l.y * r.y,
        l.z * r.z
    };
}

inline V3
operator/(const V3& v, f32 s)
{
    return {
        .x = v.x / s,
        .y = v.y / s,
        .z = v.z / s
    };
}

inline V3
operator+(V3 a, f32 b)
{
    a.x += b;
    a.y += b;
    a.z += b;
    return a;
}

inline V3&
operator+=(V3& a, f32 b)
{
    return a = a + b;
}

inline V3&
operator+=(V3& l, const V3& r)
{
    return l = l + r;
}

inline V3&
operator-=(V3& l, const V3& r)
{
    return l = l - r;
}

inline V3&
operator*=(V3& v, f32 s)
{
    return v = v * s;
}

inline V3&
operator/=(V3& v, f32 s)
{
    return v = v / s;
}

inline V4
operator+(const V4& l, const V4& r)
{
    return {
        .x = l.x + r.x,
        .y = l.y + r.y,
        .z = l.z + r.z,
        .w = l.w + r.w
    };
}

inline V4
operator-(const V4& l)
{
    V4 res;
    res.x = -l.x;
    res.y = -l.y;
    res.z = -l.z;
    res.w = -l.w;
    return res;
}

inline V4
operator-(const V4& l, const V4& r)
{
    return {
        .x = l.x - r.x,
        .y = l.y - r.y,
        .z = l.z - r.z,
        .w = l.w - r.w
    };
}

inline V4
operator*(const V4& l, f32 r)
{
    return {
        .x = l.x * r,
        .y = l.y * r,
        .z = l.z * r,
        .w = l.w * r
    };
}

inline V4
operator*(f32 l, const V4& r)
{
    return r * l;
}

inline V4
operator/(const V4& l, f32 r)
{
    return {
        .x = l.x / r,
        .y = l.y / r,
        .z = l.z / r,
        .w = l.w / r
    };
}

inline V4
operator/(f32 l, const V4& r)
{
    return r * l;
}

inline V4&
operator+=(V4& l, const V4& r)
{
    return l = l + r;
}

inline V4&
operator-=(V4& l, const V4& r)
{
    return l = l - r;
}

inline V4&
operator*=(V4& l, f32 r)
{
    return l = l * r;
}

inline V4&
operator/=(V4& l, f32 r)
{
    return l = l / r;
}

constexpr M2
M2Iden()
{
    return {
        1, 0,
        0, 1,
    };
}

constexpr M3
M3Iden()
{
    return {
        1, 0, 0,
        0, 1, 0,
        0, 0, 1
    };
}

constexpr M4
M4Iden()
{
    return {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
}

constexpr Qt
QtIden()
{
    return {0, 0, 0, 1};
}

inline f32
M2Det(const M2& s)
{
    auto& d = s.d;
    return d[0]*d[3] - d[1]*d[2];
}

inline f32
M3Det(const M3& s)
{
    auto& e = s.e;
    return (
        e[0][0] * (e[1][1] * e[2][2] - e[2][1] * e[1][2]) -
        e[0][1] * (e[1][0] * e[2][2] - e[1][2] * e[2][0]) +
        e[0][2] * (e[1][0] * e[2][1] - e[1][1] * e[2][0])
    );
}

inline f32
M4Det(const M4& s)
{
    auto& e = s.e;
    return (
        e[0][3] * e[1][2] * e[2][1] * e[3][0] - e[0][2] * e[1][3] * e[2][1] * e[3][0] -
        e[0][3] * e[1][1] * e[2][2] * e[3][0] + e[0][1] * e[1][3] * e[2][2] * e[3][0] +
        e[0][2] * e[1][1] * e[2][3] * e[3][0] - e[0][1] * e[1][2] * e[2][3] * e[3][0] -
        e[0][3] * e[1][2] * e[2][0] * e[3][1] + e[0][2] * e[1][3] * e[2][0] * e[3][1] +
        e[0][3] * e[1][0] * e[2][2] * e[3][1] - e[0][0] * e[1][3] * e[2][2] * e[3][1] -
        e[0][2] * e[1][0] * e[2][3] * e[3][1] + e[0][0] * e[1][2] * e[2][3] * e[3][1] +
        e[0][3] * e[1][1] * e[2][0] * e[3][2] - e[0][1] * e[1][3] * e[2][0] * e[3][2] -
        e[0][3] * e[1][0] * e[2][1] * e[3][2] + e[0][0] * e[1][3] * e[2][1] * e[3][2] +
        e[0][1] * e[1][0] * e[2][3] * e[3][2] - e[0][0] * e[1][1] * e[2][3] * e[3][2] -
        e[0][2] * e[1][1] * e[2][0] * e[3][3] + e[0][1] * e[1][2] * e[2][0] * e[3][3] +
        e[0][2] * e[1][0] * e[2][1] * e[3][3] - e[0][0] * e[1][2] * e[2][1] * e[3][3] -
        e[0][1] * e[1][0] * e[2][2] * e[3][3] + e[0][0] * e[1][1] * e[2][2] * e[3][3]
    );
}

inline M3
M3Minors(const M3& s)
{
    const auto& e = s.e;
    return {.d {
            M2Det({.d {e[1][1], e[1][2], e[2][1], e[2][2]} }),
            M2Det({.d {e[1][0], e[1][2], e[2][0], e[2][2]} }),
            M2Det({.d {e[1][0], e[1][1], e[2][0], e[2][1]} }),

            M2Det({.d {e[0][1], e[0][2], e[2][1], e[2][2]} }),
            M2Det({.d {e[0][0], e[0][2], e[2][0], e[2][2]} }),
            M2Det({.d {e[0][0], e[0][1], e[2][0], e[2][1]} }),

            M2Det({.d {e[0][1], e[0][2], e[1][1], e[1][2]} }),
            M2Det({.d {e[0][0], e[0][2], e[1][0], e[1][2]} }),
            M2Det({.d {e[0][0], e[0][1], e[1][0], e[1][1]} })
        }
    };
}

inline M4
M4Minors(const M4& s)
{
    const auto& e = s.e;
    return {.d {
            M3Det({.d {e[1][1], e[1][2], e[1][3],    e[2][1], e[2][2], e[2][3],    e[3][1], e[3][2], e[3][3]} }),
            M3Det({.d {e[1][0], e[1][2], e[1][3],    e[2][0], e[2][2], e[2][3],    e[3][0], e[3][2], e[3][3]} }),
            M3Det({.d {e[1][0], e[1][1], e[1][3],    e[2][0], e[2][1], e[2][3],    e[3][0], e[3][1], e[3][3]} }),
            M3Det({.d {e[1][0], e[1][1], e[1][2],    e[2][0], e[2][1], e[2][2],    e[3][0], e[3][1], e[3][2]} }),

            M3Det({.d {e[0][1], e[0][2], e[0][3],    e[2][1], e[2][2], e[2][3],    e[3][1], e[3][2], e[3][3]} }),
            M3Det({.d {e[0][0], e[0][2], e[0][3],    e[2][0], e[2][2], e[2][3],    e[3][0], e[3][2], e[3][3]} }),
            M3Det({.d {e[0][0], e[0][1], e[0][3],    e[2][0], e[2][1], e[2][3],    e[3][0], e[3][1], e[3][3]} }),
            M3Det({.d {e[0][0], e[0][1], e[0][2],    e[2][0], e[2][1], e[2][2],    e[3][0], e[3][1], e[3][2]} }),

            M3Det({.d {e[0][1], e[0][2], e[0][3],    e[1][1], e[1][2], e[1][3],    e[3][1], e[3][2], e[3][3]} }),
            M3Det({.d {e[0][0], e[0][2], e[0][3],    e[1][0], e[1][2], e[1][3],    e[3][0], e[3][2], e[3][3]} }),
            M3Det({.d {e[0][0], e[0][1], e[0][3],    e[1][0], e[1][1], e[1][3],    e[3][0], e[3][1], e[3][3]} }),
            M3Det({.d {e[0][0], e[0][1], e[0][2],    e[1][0], e[1][1], e[1][2],    e[3][0], e[3][1], e[3][2]} }),

            M3Det({.d {e[0][1], e[0][2], e[0][3],    e[1][1], e[1][2], e[1][3],    e[2][1], e[2][2], e[2][3]} }),
            M3Det({.d {e[0][0], e[0][2], e[0][3],    e[1][0], e[1][2], e[1][3],    e[2][0], e[2][2], e[2][3]} }),
            M3Det({.d {e[0][0], e[0][1], e[0][3],    e[1][0], e[1][1], e[1][3],    e[2][0], e[2][1], e[2][3]} }),
            M3Det({.d {e[0][0], e[0][1], e[0][2],    e[1][0], e[1][1], e[1][2],    e[2][0], e[2][1], e[2][2]} })
        }
    };
}

inline M3
M3Cofactors(const M3& s)
{
    M3 m = M3Minors(s);
    auto& e = m.e;

    e[0][0] *= +1, e[0][1] *= -1, e[0][2] *= +1;
    e[1][0] *= -1, e[1][1] *= +1, e[1][2] *= -1;
    e[2][0] *= +1, e[2][1] *= -1, e[2][2] *= +1;

    return m;
}

inline M4
M4Cofactors(const M4& s)
{
    M4 m = M4Minors(s);
    auto& e = m.e;

    e[0][0] *= +1, e[0][1] *= -1, e[0][2] *= +1, e[0][3] *= -1;
    e[1][0] *= -1, e[1][1] *= +1, e[1][2] *= -1, e[1][3] *= +1;
    e[2][0] *= +1, e[2][1] *= -1, e[2][2] *= +1, e[2][3] *= -1;
    e[3][0] *= -1, e[3][1] *= +1, e[3][2] *= -1, e[3][3] *= +1;

    return m;
}

inline M3
M3Transpose(const M3& s)
{
    auto& e = s.e;
    return {
        e[0][0], e[1][0], e[2][0],
        e[0][1], e[1][1], e[2][1],
        e[0][2], e[1][2], e[2][2]
    };
}

inline M4
M4Transpose(const M4& s)
{
    auto& e = s.e;
    return {
        e[0][0], e[1][0], e[2][0], e[3][0],
        e[0][1], e[1][1], e[2][1], e[3][1],
        e[0][2], e[1][2], e[2][2], e[3][2],
        e[0][3], e[1][3], e[2][3], e[3][3]
    };
}

inline M3
M3Adj(const M3& s)
{
    return M3Transpose(M3Cofactors(s));
}

inline M4
M4Adj(const M4& s)
{
    return M4Transpose(M4Cofactors(s));
}

inline M3
operator*(const M3& l, const f32 r)
{
    M3 m {};

    for (int i = 0; i < 9; ++i)
        m.d[i] = l.d[i] * r;

    return m;
}

inline M4
operator*(const M4& l, const f32 r)
{
    M4 m {};

    for (int i = 0; i < 16; ++i)
        m.d[i] = l.d[i] * r;

    return m;
}

inline M4
operator*(const M4& a, bool)
{
    ADT_ASSERT(false, "mul with bool is no good");
    return a;
}

inline M3&
operator*=(M3& l, const f32 r)
{
    for (int i = 0; i < 9; ++i)
        l.d[i] *= r;

    return l;
}

inline M4&
operator*=(M4& l, const f32 r)
{
    for (int i = 0; i < 16; ++i)
        l.d[i] *= r;

    return l;
}

inline M4&
operator*=(M4& a, bool)
{
    ADT_ASSERT(false, "mul with bool is no good");
    return a;
}

inline M3
operator*(const f32 l, const M3& r)
{
    return r * l;
}

inline M4
operator*(const f32 l, const M4& r)
{
    return r * l;
}

inline M3
M3Inv(const M3& s)
{
    return (1.0f/M3Det(s)) * M3Adj(s);
}

inline M4
M4Inv(const M4& s)
{
    return (1.0f/M4Det(s)) * M4Adj(s);
}

inline M3 
M3Normal(const M3& m)
{
    return M3Transpose(M3Inv(m));
}

inline V3
operator*(const M3& l, const V3& r)
{
    return l.v[0] * r.x + l.v[1] * r.y + l.v[2] * r.z;
}

inline V4
operator*(const M4& l, const V4& r)
{
    return l.v[0] * r.x + l.v[1] * r.y + l.v[2] * r.z + l.v[3] * r.w;
}

inline M3
operator*(const M3& l, const M3& r)
{
    M3 m {};

    m.v[0] = l * r.v[0];
    m.v[1] = l * r.v[1];
    m.v[2] = l * r.v[2];

    return m;
}

inline M3&
operator*=(M3& l, const M3& r)
{
    return l = l * r;
}

inline M4
operator*(const M4& l, const M4& r)
{
    M4 m {};

    m.v[0] = l * r.v[0];
    m.v[1] = l * r.v[1];
    m.v[2] = l * r.v[2];
    m.v[3] = l * r.v[3];

    return m;
}

inline M4&
operator*=(M4& l, const M4& r)
{
    return l = l * r;
}

inline bool
operator==(const V3& l, const V3& r)
{
    for (int i = 0; i < 3; ++i)
        if (!eq(l.e[i], r.e[i]))
            return false;

    return true;
}

inline bool
operator==(const V4& l, const V4& r)
{
    for (int i = 0; i < 4; ++i)
        if (!eq(l.e[i], r.e[i]))
            return false;

    return true;
}

inline bool
operator==(const M3& l, const M3& r)
{
    for (int i = 0; i < 9; ++i)
        if (!eq(l.d[i], r.d[i]))
            return false;

    return true;
}

inline bool
operator==(const M4& l, const M4& r)
{
    for (int i = 0; i < 16; ++i)
        if (!eq(l.d[i], r.d[i]))
            return false;

    return true;
}

inline f32
V2Length(const V2& s)
{
    return hypotf(s.x, s.y);
}

inline f32
V3Length(const V3& s)
{
    return sqrtf(sq(s.x) + sq(s.y) + sq(s.z));
}

inline f32
V4Length(const V4& s)
{
    return sqrtf(sq(s.x) + sq(s.y) + sq(s.z) + sq(s.w));
}

inline V2
V2Norm(const V2& s)
{
    f32 len = V2Length(s);
    return V2 {s.x / len, s.y / len};
}

inline V3
V3Norm(const V3& s, const f32 len)
{
    return V3 {s.x / len, s.y / len, s.z / len};
}

inline V3
V3Norm(const V3& s)
{
    f32 len = V3Length(s);
    return V3Norm(s, len);
}

inline V4
V4Norm(const V4& s)
{
    f32 len = V4Length(s);
    return {s.x / len, s.y / len, s.z / len, s.w / len};
}

inline V2
V2Clamp(const V2& x, const V2& min, const V2& max)
{
    V2 r {};

    f32 minX = utils::min(min.x, max.x);
    f32 minY = utils::min(min.y, max.y);

    f32 maxX = utils::max(min.x, max.x);
    f32 maxY = utils::max(min.y, max.y);

    r.x = utils::clamp(x.x, minX, maxX);
    r.y = utils::clamp(x.y, minY, maxY);

    return r;
}

inline f32
V2Dot(const V2& l, const V2& r)
{
    return (l.x * r.x) + (l.y * r.y);
}

inline f32
V3Dot(const V3& l, const V3& r)
{
    return (l.x * r.x) + (l.y * r.y) + (l.z * r.z);
}

inline f32
V4Dot(const V4& l, const V4& r)
{
    return (l.x * r.x) + (l.y * r.y) + (l.z * r.z) + (l.w * r.w);
}

inline f32
V3Rad(const V3& l, const V3& r)
{
    return std::acos(V3Dot(l, r) / (V3Length(l) * V3Length(r)));
}

inline f32
V2Dist(const V2& l, const V2& r)
{
    return sqrtf(sq(r.x - l.x) + sq(r.y - l.y));
}

inline f32
V3Dist(const V3& l, const V3& r)
{
    return sqrtf(sq(r.x - l.x) + sq(r.y - l.y) + sq(r.z - l.z));
}

constexpr M4
M4TranslationFrom(const V3& tv)
{
    return {
        1,    0,    0,    0,
        0,    1,    0,    0,
        0,    0,    1,    0,
        tv.x, tv.y, tv.z, 1
    };
}

constexpr M4
M4TranslationFrom(const f32 x, const f32 y, const f32 z)
{
    return M4TranslationFrom(V3{x, y, z});
}

inline M4
M4Translate(const M4& m, const V3& tv)
{
    return m * M4TranslationFrom(tv);
}

inline M3
M3Scale(const M3& m, const f32 s)
{
    M3 sm {
        s, 0, 0,
        0, s, 0,
        0, 0, 1
    };

    return m * sm;
}

constexpr M4
M4ScaleFrom(const f32 s)
{
    return {
        s, 0, 0, 0,
        0, s, 0, 0,
        0, 0, s, 0,
        0, 0, 0, 1
    };
}

constexpr M4
M4ScaleFrom(const V3& v)
{
    return {
        v.x, 0,   0,   0,
        0,   v.y, 0,   0,
        0,   0,   v.z, 0,
        0,   0,   0,   1
    };
}

constexpr M4
M4ScaleFrom(f32 x, f32 y, f32 z)
{
    return M4ScaleFrom({x, y, z});
}

inline M4
M4Scale(const M4& m, const f32 s)
{
    return m * M4ScaleFrom(s);
}

inline M3
M3Scale(const M3& m, const V2& s)
{
    M3 sm {
        s.x, 0,   0,
        0,   s.y, 0,
        0,   0,   1
    };

    return m * sm;
}

inline M4
M4Scale(const M4& m, const V3& s)
{
    return m * M4ScaleFrom(s);
}

inline M4
M4Pers(const f32 fov, const f32 asp, const f32 n, const f32 f)
{
    M4 res {};
    res.v[0].x = 1.0f / (asp * std::tan(fov * 0.5f));
    res.v[1].y = 1.0f / (std::tan(fov * 0.5f));
    res.v[2].z = -f / (n - f);
    res.v[3].z = n * f / (n - f);
    res.v[2].w = 1.0f;

    return res;
}

inline M4
M4Ortho(const f32 l, const f32 r, const f32 b, const f32 t, const f32 n, const f32 f)
{
    return {
        2/(r-l),       0,            0,           0,
        0,             2/(t-b),      0,           0,
        0,             0,           -2/(f-n),     0,
        -(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1
    };
}

inline f32
V2Cross(const V2& l, const V2& r)
{
    return l.x * r.y - l.y * r.x;
}

inline i64
IV2Cross(const IV2& l, const IV2& r)
{
    return i64(l.x) * i64(r.y) - i64(l.y) * i64(r.x);
}

inline V3
V3Cross(const V3& l, const V3& r)
{
    return {
        (l.y * r.z) - (r.y * l.z),
        (l.z * r.x) - (r.z * l.x),
        (l.x * r.y) - (r.x * l.y)
    };
}

inline M4
M4LookAt(const V3& R, const V3& U, const V3& D, const V3& P)
{
    M4 m0 {
        R.x,  U.x,  D.x,  0,
        R.y,  U.y,  D.y,  0,
        R.z,  U.z,  D.z,  0,
        0,    0,    0,    1
    };

    return (M4Translate(m0, {-P.x, -P.y, -P.z}));
}

inline M4
M4RotFrom(const f32 th, const V3& ax)
{
    const f32 c = std::cos(th);
    const f32 s = std::sin(th);

    const f32 x = ax.x;
    const f32 y = ax.y;
    const f32 z = ax.z;

    return {
        ((1 - c)*sq(x)) + c, ((1 - c)*x*y) - s*z, ((1 - c)*x*z) + s*y, 0,
        ((1 - c)*x*y) + s*z, ((1 - c)*sq(y)) + c, ((1 - c)*y*z) - s*x, 0,
        ((1 - c)*x*z) - s*y, ((1 - c)*y*z) + s*x, ((1 - c)*sq(z)) + c, 0,
        0,                   0,                   0,                   1
    };
}

inline M4
M4Rot(const M4& m, const f32 th, const V3& ax)
{
    return m * M4RotFrom(th, ax);
}

inline M4
M4RotXFrom(const f32 th)
{
    return {
        1,  0,            0,            0,
        0,  std::cos(th), std::sin(th), 0,
        0, -std::sin(th), std::cos(th), 0,
        0,  0,            0,            1
    };
}

inline M4
M4RotX(const M4& m, const f32 th)
{
    return m * M4RotXFrom(th);
}

inline M4
M4RotYFrom(const f32 th)
{
    return {
        std::cos(th), 0,  std::sin(th),  0,
        0,            1,  0,             0,
       -std::sin(th), 0,  std::cos(th),  0,
        0,            0,  0,             1
    };
}

inline M4
M4RotY(const M4& m, const f32 th)
{
    return m * M4RotYFrom(th);
}

inline M4
M4RotZFrom(const f32 th)
{
    return {
        std::cos(th),  std::sin(th), 0, 0,
       -std::sin(th),  std::cos(th), 0, 0,
        0,             0,            1, 0,
        0,             0,            0, 1
    };
}

inline M4
M4RotZ(const M4& m, const f32 th)
{
    return m * M4RotZFrom(th);
}

inline M4
M4RotFrom(const f32 x, const f32 y, const f32 z)
{
    return M4RotZFrom(z) * M4RotYFrom(y) * M4RotXFrom(x);
}

inline M4
M4LookAt(const V3& eyeV, const V3& centerV, const V3& upV)
{
    V3 camDir = V3Norm(eyeV - centerV);
    V3 camRight = V3Norm(V3Cross(upV, camDir));
    V3 camUp = V3Cross(camDir, camRight);

    return M4LookAt(camRight, camUp, camDir, eyeV);
}

inline Qt
QtAxisAngle(const V3& axis, f32 th)
{
    f32 sinTh = std::sin(th / 2.0f);

    return {
        axis.x * sinTh,
        axis.y * sinTh,
        axis.z * sinTh,
        std::cos(th / 2.0f)
    };
}

inline M4
QtRot(const Qt& q)
{
    auto& x = q.x;
    auto& y = q.y;
    auto& z = q.z;
    auto& s = q.w;

    return {
        1 - 2*y*y - 2*z*z, 2*x*y - 2*s*z,     2*x*z + 2*s*y,     0,
        2*x*y + 2*s*z,     1 - 2*x*x - 2*z*z, 2*y*z - 2*s*x,     0,
        2*x*z - 2*s*y,     2*y*z + 2*s*x,     1 - 2*x*x - 2*y*y, 0,
        0,                 0,                 0,                 1
    };
}

inline Qt
QtConj(const Qt& q)
{
    return {-q.x, -q.y, -q.z, q.w};
}

inline Qt
operator-(const Qt& l)
{
    Qt res;
    res.base = -l.base;
    return res;
}

inline Qt
operator*(const Qt& l, const Qt& r)
{
    return {
        l.w*r.x + l.x*r.w + l.y*r.z - l.z*r.y,
        l.w*r.y - l.x*r.z + l.y*r.w + l.z*r.x,
        l.w*r.z + l.x*r.y - l.y*r.x + l.z*r.w,
        l.w*r.w - l.x*r.x - l.y*r.y - l.z*r.z,
    };
}

inline Qt
operator*(const Qt& l, const V4& r)
{
    return l * (*(Qt*)&r);
}

inline Qt
operator*=(Qt& l, const Qt& r)
{
    return l = l * r;
}

inline Qt
operator*=(Qt& l, const V4& r)
{
    return l = l * r;
}

inline bool
operator==(const Qt& a, const Qt& b)
{
    return a.base == b.base;
}

inline Qt
QtNorm(Qt a)
{
    f32 mag = std::sqrt(a.w * a.w + a.x * a.x + a.y * a.y + a.z * a.z);
    return {a.x / mag, a.y / mag, a.z / mag, a.w / mag};
}

inline V2
normalize(const V2& v)
{
    return V2Norm(v);
}

inline V3
normalize(const V3& v)
{
    return V3Norm(v);
}

inline V4
normalize(const V4& v)
{
    return V4Norm(v);
}

constexpr inline auto
lerp(auto& a, auto& b, auto& t)
{
    return (1.0 - t) * a + t * b;
}

inline Qt
slerp(const Qt& q1, const Qt& q2, f32 t)
{
    auto dot = V4Dot(q1.base, q2.base);

    Qt q2b = q2;
    if (dot < 0.0f)
    {
        q2b = -q2b;
        dot = -dot;
    }

    if (dot > 0.9995f)
    {
        Qt res;
        res.x = q1.x + t * (q2b.x - q1.x);
        res.y = q1.y + t * (q2b.y - q1.y);
        res.z = q1.z + t * (q2b.z - q1.z);
        res.w = q1.w + t * (q2b.w - q1.w);
        return QtNorm(res);
    }

    f32 theta0 = std::acos(dot);
    f32 theta = theta0 * t;

    f32 sinTheta0 = std::sin(theta0);
    f32 sinTheta = std::sin(theta);

    f32 s1 = std::cos(theta) - dot * (sinTheta / sinTheta0);
    f32 s2 = sinTheta / sinTheta0;

    Qt res;
    res.x = (s1 * q1.x) + (s2 * q2b.x);
    res.y = (s1 * q1.y) + (s2 * q2b.y);
    res.z = (s1 * q1.z) + (s2 * q2b.z);
    res.w = (s1 * q1.w) + (s2 * q2b.w);
    return res;
}

template<typename T>
constexpr T
bezier(
    const T& p0,
    const T& p1,
    const T& p2,
    const std::floating_point auto t)
{
    return sq(1-t)*p0 + 2*(1-t)*t*p1 + sq(t)*p2;
}

template<typename T>
constexpr T
bezier(
    const T& p0,
    const T& p1,
    const T& p2,
    const T& p3,
    const std::floating_point auto t)
{
    return cube(1-t)*p0 + 3*sq(1-t)*t*p1 + 3*(1-t)*sq(t)*p2 + cube(t)*p3;
}

} /* namespace adt::math */

namespace adt::print
{

inline ssize
formatToContext(Context ctx, FormatArgs, const math::V2& x)
{
    ctx.fmt = "{:.3}, {:.3}";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.x, x.y);
}

inline ssize
formatToContext(Context ctx, FormatArgs, const math::V3& x)
{
    ctx.fmt = "{:.3}, {:.3}, {:.3}";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.x, x.y, x.z);
}

inline ssize
formatToContext(Context ctx, FormatArgs, const math::V4& x)
{
    ctx.fmt = "{:.3}, {:.3}, {:.3}, {:.3}";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.x, x.y, x.z, x.w);
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const math::IV4& x)
{
    i32 aBuff[4] {x.x, x.y, x.z, x.w};
    return formatToContext(ctx, fmtArgs, aBuff);
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const math::IV4u16& x)
{
    u16 aBuff[4] {x.x, x.y, x.z, x.w};
    return formatToContext(ctx, fmtArgs, aBuff);
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const math::Qt& x)
{
    return formatToContext(ctx, fmtArgs, x.base);
}

inline ssize
formatToContext(Context ctx, FormatArgs, const math::M2& x)
{
    ctx.fmt = "\n\t[{:.3}, {:.3}"
              "\n\t {:.3}, {:.3}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.d[0], x.d[1], x.d[2], x.d[3]);
}

inline ssize
formatToContext(Context ctx, FormatArgs, const math::M3& x)
{
    ctx.fmt = "\n\t[{:.3}, {:.3}, {:.3}"
              "\n\t {:.3}, {:.3}, {:.3}"
              "\n\t {:.3}, {:.3}, {:.3}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx,
        x.d[0], x.d[1], x.d[2],
        x.d[3], x.d[4], x.d[5],
        x.d[6], x.d[7], x.d[8]
    );
}

inline ssize
formatToContext(Context ctx, FormatArgs, const math::M4& x)
{
    ctx.fmt = "\n\t[{:.3}, {:.3}, {:.3}, {:.3}"
              "\n\t {:.3}, {:.3}, {:.3}, {:.3}"
              "\n\t {:.3}, {:.3}, {:.3}, {:.3}"
              "\n\t {:.3}, {:.3}, {:.3}, {:.3}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx,
        x.d[0], x.d[1], x.d[2], x.d[3],
        x.d[4], x.d[5], x.d[6], x.d[7],
        x.d[8], x.d[9], x.d[10], x.d[11],
        x.d[12], x.d[13], x.d[14], x.d[15]
    );
}

} /* namespace adt::print */

#if defined __clang__
    #pragma clang diagnostic pop
#endif

#if defined __GNUC__
    #pragma GCC diagnostic pop
#endif

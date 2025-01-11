#pragma once

#include "adt/print.hh"
#include "adt/utils.hh"
#include "adt/types.hh"

#include <cassert>
#include <cmath>
#include <concepts>
#include <limits>

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-braces"
#elif defined __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-braces"
#endif

namespace adt
{
namespace math
{

constexpr f64 PI64 = 3.14159265358979323846;
constexpr f32 PI32 = f32(PI64);
constexpr f64 EPS64 = std::numeric_limits<f64>::epsilon();
constexpr f32 EPS32 = std::numeric_limits<f32>::epsilon();

constexpr f64 toDeg(f64 x) { return x * 180.0 / PI64; }
constexpr f64 toRad(f64 x) { return x * PI64 / 180.0; }
constexpr f32 toDeg(f32 x) { return x * 180.0f / PI32; }
constexpr f32 toRad(f32 x) { return x * PI32 / 180.0f; }

constexpr f64 toRad(long x) { return toRad(f64(x)); }
constexpr f64 toDeg(long x) { return toDeg(f64(x)); }
constexpr f32 toRad(int x) { return toRad(f32(x)); }
constexpr f32 toDeg(int x) { return toDeg(f32(x)); }

/* epsilon float comparison */
inline bool
eq(f64 l, f64 r)
{
    return std::abs(l - r) <= EPS64*(std::abs(l) + std::abs(r) + 1.0);
}

/* epsilon float comparison */
inline bool
eq(f32 l, f32 r)
{
    return std::abs(l - r) <= EPS32*(std::abs(l) + std::abs(r) + 1.0f);
}

constexpr auto sq(const auto& x) { return x * x; }
constexpr auto cube(const auto& x) { return x*x*x; }

union V2
{
    f32 e[2];
    struct {
        f32 x, y;
    };
    struct {
        f32 u, v;
    };
};

union V3
{
    f32 e[3];
    struct {
        V2 xy;
        f32 __v2pad;
    };
    struct {
        f32 x, y, z;
    };
    struct {
        f32 r, g, b;
    };
};

union V4
{
    f32 e[4];
    struct {
        V3 xyz;
        f32 __v3pad;
    };
    struct {
        V2 xy;
        V2 zw;
    };
    struct {
        f32 x, y, z, w;
    };
    struct {
        f32 r, g, b, a;
    };
};

union M2
{
    f32 d[4];
    f32 e[2][2];
    V2 v[2];
};

union M3
{
    f32 d[9];
    f32 e[3][3];
    V3 v[3];
};

union M4
{
    f32 d[16];
    f32 e[4][4];
    V4 v[4];
};

union Qt
{
    V4 base;
    f32 e[4];
    struct {
        f32 x, y, z, w;
    };
};

inline M3
M4ToM3(const M4& s)
{
    auto& e = s.e;
    return {
        e[0][0], e[0][1], e[0][2],
        e[1][0], e[1][1], e[1][2],
        e[2][0], e[2][1], e[2][2]
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
operator*(const V3& v, f32 s)
{
    return {
        .x = v.x * s,
        .y = v.y * s,
        .z = v.z * s
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
operator/(const V4& l, f32 r)
{
    return {
        .x = l.x / r,
        .y = l.y / r,
        .z = l.z / r,
        .w = l.w / r
    };
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

    e[0][0] *= +1.0f, e[0][1] *= -1, e[0][2] *= +1, e[0][3] *= -1;
    e[1][0] *= -1.0f, e[1][1] *= +1, e[1][2] *= -1, e[1][3] *= +1;
    e[2][0] *= +1.0f, e[2][1] *= -1, e[2][2] *= +1, e[2][3] *= -1;
    e[3][0] *= -1.0f, e[3][1] *= +1, e[3][2] *= -1, e[3][3] *= +1;

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

inline M3
operator*(const M3& l, const M3& r)
{
    M3 m {};

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                m.e[j][i] += l.e[k][i] * r.e[j][k];

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

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                m.e[j][i] += l.e[k][i] * r.e[j][k];

    return m;
}

inline V4
operator*(const M4& l, const V4& r)
{
    V4 res {};

    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++)
            res.e[j] += l.e[j][i] * r.e[i];

    return res;
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
V3Norm(const V3& s)
{
    f32 len = V3Length(s);
    return V3 {s.x / len, s.y / len, s.z / len};
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

inline M4
M4Translate(const M4& m, const V3& tv)
{
    M4 tm {
        1,    0,    0,    0,
        0,    1,    0,    0,
        0,    0,    1,    0,
        tv.x, tv.y, tv.z, 1
    };

    return m * tm;
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

inline M4
M4Scale(const M4& m, const f32 s)
{
    M4 sm {
        s, 0, 0, 0,
        0, s, 0, 0,
        0, 0, s, 0,
        0, 0, 0, 1
    };

    return m * sm;
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
    M4 sm {
        s.x, 0,   0,   0,
        0,   s.y, 0,   0,
        0,   0,   s.z, 0,
        0,   0,   0,   1
    };

    return m * sm;
}

inline M4
M4Pers(const f32 fov, const f32 asp, const f32 n, const f32 f)
{
    /* b(back), l(left) are not needed if viewing volume is symmetric */
    f32 t = n * std::tan(fov / 2);
    f32 r = t * asp;

    return M4 {
        n / r, 0,     0,                  0,
        0,     n / t, 0,                  0,
        0,     0,    -(f + n) / (f - n), -1,
        0,     0,    -(2*f*n) / (f - n),  0
    };
}

inline M4
M4Ortho(const f32 l, const f32 r, const f32 b, const f32 t, const f32 n, const f32 f)
{
    return M4 {
        2/(r-l),       0,            0,           0,
        0,             2/(t-b),      0,           0,
        0,             0,           -2/(f-n),     0,
        -(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1
    };
}

inline V3
V3Cross(const V3& l, const V3& r)
{
    return V3 {
        (l.y * r.z) - (r.y * l.z),
        (l.z * r.x) - (r.z * l.x),
        (l.x * r.y) - (r.x * l.y)
    };
}

inline M4
m4LookAt(const V3& R, const V3& U, const V3& D, const V3& P)
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
M4Rot(const M4& m, const f32 th, const V3& ax)
{
    const f32 c = std::cos(th);
    const f32 s = std::sin(th);

    const f32 x = ax.x;
    const f32 y = ax.y;
    const f32 z = ax.z;

    M4 r {
        ((1 - c)*sq(x)) + c, ((1 - c)*x*y) - s*z, ((1 - c)*x*z) + s*y, 0,
        ((1 - c)*x*y) + s*z, ((1 - c)*sq(y)) + c, ((1 - c)*y*z) - s*x, 0,
        ((1 - c)*x*z) - s*y, ((1 - c)*y*z) + s*x, ((1 - c)*sq(z)) + c, 0,
        0,                   0,                   0,                   1
    };

    return m * r;
}

inline M4
M4RotX(const M4& m, const f32 angle)
{
    M4 axisX {
        1, 0,                0,               0,
        0, std::cos(angle),  std::sin(angle), 0,
        0, -std::sin(angle), std::cos(angle), 0,
        0, 0,                0,               1
    };

    return m * axisX;
}

inline M4
M4RotY(const M4& m, const f32 angle)
{
    M4 axisY {
        std::cos(angle), 0, -std::sin(angle), 0,
        0,               1, 0,                0,
        std::sin(angle), 0, std::cos(angle),  0,
        0,               0, 0,                1
    };

    return m * axisY;
}

inline M4
M4RotZ(const M4& m, const f32 angle)
{
    M4 axisZ {
        std::cos(angle),  std::sin(angle), 0, 0,
        -std::sin(angle), std::cos(angle), 0, 0,
        0,                0,               1, 0,
        0,                0,               0, 1
    };

    return m * axisZ;
}

inline M4
M4LookAt(const V3& eyeV, const V3& centerV, const V3& upV)
{
    V3 camDir = V3Norm(eyeV - centerV);
    V3 camRight = V3Norm(V3Cross(upV, camDir));
    V3 camUp = V3Cross(camDir, camRight);

    return m4LookAt(camRight, camUp, camDir, eyeV);
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
    return {
        l.w*r.x + l.x*r.w + l.y*r.z - l.z*r.y,
        l.w*r.y - l.x*r.z + l.y*r.w + l.z*r.x,
        l.w*r.z + l.x*r.y - l.y*r.x + l.z*r.w,
        l.w*r.w - l.x*r.x - l.y*r.y - l.z*r.z,
    };
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

template<typename T>
constexpr T
lerp(const T& l, const T& r, const std::floating_point auto t)
{
    return l + (r - l)*t;
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

} /* namespace math */
} /* namespace adt */

namespace adt
{
namespace print
{

inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const math::V2& x)
{
    ctx.fmt = "[{:.3}, {:.3}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.x, x.y);
}

inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const math::V3& x)
{
    ctx.fmt = "[{:.3}, {:.3}, {:.3}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.x, x.y, x.z);
}

inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const math::V4& x)
{
    ctx.fmt = "[{:.3}, {:.3}, {:.3}, {:.3}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.x, x.y, x.z, x.w);
}

inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const math::M2& x)
{
    ctx.fmt = "\n\t[{:.3}, {:.3}"
              "\n\t {:.3}, {:.3}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.d[0], x.d[1], x.d[2], x.d[3]);
}

inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const math::M3& x)
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
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const math::M4& x)
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

} /* namespace print */
} /* namespace adt */

#if defined __clang__
    #pragma clang diagnostic pop
#elif defined __GNUC__
    #pragma GCC diagnostic pop
#endif

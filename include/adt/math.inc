#pragma once

#include "assert.hh"

#include <concepts>
#include <limits>

namespace adt::math
{

constexpr f64 PI64 = 3.14159265358979323846;
constexpr f32 PI32 = static_cast<f32>(PI64);
constexpr f64 EPS64 = std::numeric_limits<f64>::epsilon();
constexpr f32 EPS32 = std::numeric_limits<f32>::epsilon();

constexpr inline f64 toDeg(f64 x);
constexpr inline f64 toRad(f64 x);
constexpr inline f32 toDeg(f32 x);
constexpr inline f32 toRad(f32 x);

constexpr inline f64 toRad(i64 x);
constexpr inline f64 toDeg(i64 x);
constexpr inline f32 toRad(i32 x);
constexpr inline f32 toDeg(i32 x);

inline bool eq(const f64 l, const f64 r);
inline bool eq(const f32 l, const f32 r);

constexpr inline auto sq(const auto& x);
constexpr inline auto cube(const auto& x);

constexpr inline i64 sign(i64 x);

union IV2;

union V2
{
    f32 e[2];
    struct { f32 x, y; };
    struct { f32 u, v; };

    constexpr explicit operator IV2() const;

    f32& operator[](int i) { ADT_ASSERT(i >= 0 && i < 2, "i: {}", i); return e[i]; }
    const f32& operator[](int i) const { ADT_ASSERT(i >= 0 && i < 2, "i: {}", i); return e[i]; }
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

    int& operator[](int i) { ADT_ASSERT(i >= 0 && i < 2, "i: {}", i); return e[i]; }
    const int& operator[](int i) const { ADT_ASSERT(i >= 0 && i < 2, "i: {}", i); return e[i]; }
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

    f32& operator[](int i) { ADT_ASSERT(i >= 0 && i < 3, "i: {}", i); return e[i]; }
    const f32& operator[](int i) const { ADT_ASSERT(i >= 0 && i < 3, "i: {}", i); return e[i]; }
};

union IV3
{
    int e[3];
    struct { IV2 xy; int _v2pad; };
    struct { int x, y, z; };
    struct { int r, g, b; };

    int& operator[](int i) { ADT_ASSERT(i >= 0 && i < 3, "i: {}", i); return e[i]; }
    const int& operator[](int i) const { ADT_ASSERT(i >= 0 && i < 3, "i: {}", i); return e[i]; }
};

union IV4;
union Qt;

union V4
{
    f32 e[4];
    struct { V3 xyz; f32 _v3pad; };
    struct { V2 xy; V2 zw; };
    struct { f32 x, y, z, w; };
    struct { f32 r, g, b, a; };

    /* */

    constexpr explicit operator IV4() const;
    explicit operator Qt() const;

    f32& operator[](int i) { ADT_ASSERT(i >= 0 && i < 4, "i: {}", i); return e[i]; }
    const f32& operator[](int i) const { ADT_ASSERT(i >= 0 && i < 4, "i: {}", i); return e[i]; }
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

    int& operator[](int i) { ADT_ASSERT(i >= 0 && i < 4, "i: {}", i); return e[i]; }
    const int& operator[](int i) const { ADT_ASSERT(i >= 0 && i < 4, "i: {}", i); return e[i]; }
};

union IV4u8
{
    u8 e[4];
    struct { u8 x, y, z, w; };

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

    V4& operator[](int i) { ADT_ASSERT(i >= 0 && i < 4, "i: {}", i); return v[i]; }
    const V4& operator[](int i) const { ADT_ASSERT(i >= 0 && i < 4, "i: {}", i); return v[i]; }
};

union Qt
{
    V4 base;
    f32 e[4];
    struct { f32 x, y, z, w; };

    /* */

    Qt
    getSwapped()
    {
        Qt ret {.x = w, .y = z, .z = y, .w = x};
        return ret;
    }
};

constexpr V2 V2From(const f32 x, const f32 y);

constexpr V3 V3From(V2 xy, f32 z);

constexpr V3 V3From(f32 x, V2 yz);

constexpr V3 V3From(f32 x, f32 y, f32 z);

constexpr V3 V3From(const V3& v);

constexpr V3 V3From(const V4& v);

constexpr V4 V4From(const V4& v);

constexpr V4 V4From(const V3& xyz, f32 w);

constexpr V4 V4From(const V2& xy, const V2& zw);

constexpr V4 V4From(f32 x, const V3& yzw);

constexpr V4 V4From(f32 x, f32 y, f32 z, f32 w);

constexpr V4 V4From(f32 x);

inline IV2 IV2_F24_8(const V2 v);

inline V2 operator-(const V2& s);

inline V2 operator+(const V2& l, const V2& r);

inline V2 operator-(const V2& l, const V2& r);

inline IV2 operator-(const IV2& l, const IV2& r);

inline IV2& operator+=(IV2& l, const IV2& r);

inline IV2& operator-=(IV2& l, const IV2& r);

inline V2 operator*(const V2& v, f32 s);

inline V2 operator*(f32 s, const V2& v);

inline V2 operator*(const V2& l, const V2& r);

inline V2& operator*=(V2& l, const V2& r);

inline V2 operator/(const V2& v, f32 s);

inline V2& operator+=(V2& l, const V2& r);

inline V2& operator-=(V2& l, const V2& r);

inline V2& operator*=(V2& l, f32 r);

inline V2& operator/=(V2& l, f32 r);

inline V3 operator+(const V3& l, const V3& r);

inline V3 operator-(const V3& l, const V3& r);

inline V3 operator-(const V3& v);

inline V3 operator*(const V3& v, f32 s);

inline V3 operator*(f32 s, const V3& v);

inline V3 operator*(const V3& l, const V3& r);

inline V3 operator/(const V3& v, f32 s);

inline V3 operator+(V3 a, f32 b);

inline V3& operator+=(V3& a, f32 b);

inline V3& operator+=(V3& l, const V3& r);

inline V3& operator-=(V3& l, const V3& r);

inline V3& operator*=(V3& v, f32 s);

inline V3& operator/=(V3& v, f32 s);

inline V4 operator+(const V4& l, const V4& r);

inline V4 operator-(const V4& l);

inline V4 operator-(const V4& l, const V4& r);

inline V4 operator*(const V4& l, f32 r);

inline V4 operator*(f32 l, const V4& r);

inline V4 operator*(const V4& l, const V4& r);

inline V4& operator*=(V4& l, const V4& r);

inline V4 operator/(const V4& l, f32 r);

inline V4 operator/(f32 l, const V4& r);

inline V4& operator+=(V4& l, const V4& r);

inline V4& operator-=(V4& l, const V4& r);

inline V4& operator*=(V4& l, f32 r);

inline V4& operator/=(V4& l, f32 r);

constexpr M2 M2Iden();

constexpr M3 M3Iden();

constexpr M4 M4Iden();

constexpr Qt QtIden();

inline f32 M2Det(const M2& s);

inline f32 M3Det(const M3& s);

inline f32 M4Det(const M4& s);

inline M3 M3Minors(const M3& s);

inline M4 M4Minors(const M4& s);

inline M3 M3Cofactors(const M3& s);

inline M4 M4Cofactors(const M4& s);

inline M3 M3Transpose(const M3& s);

inline M4 M4Transpose(const M4& s);

inline M3 M3Adj(const M3& s);

inline M4 M4Adj(const M4& s);

inline M3 operator*(const M3& l, const f32 r);

inline M4 operator*(const M4& l, const f32 r);

inline M4 operator*(const M4& a, bool) = delete;

inline M3& operator*=(M3& l, const f32 r);

inline M4& operator*=(M4& l, const f32 r);

inline M4& operator*=(M4& a, bool);

inline M3 operator*(const f32 l, const M3& r);

inline M4 operator*(const f32 l, const M4& r);

inline M3 M3Inv(const M3& s);

inline M4 M4Inv(const M4& s);

inline M3 M3Normal(const M3& m);

inline V3 operator*(const M3& l, const V3& r);

inline V4 operator*(const M4& l, const V4& r);

inline M3 operator*(const M3& l, const M3& r);

inline M3& operator*=(M3& l, const M3& r);

inline M4 operator*(const M4& l, const M4& r);

inline M4& operator*=(M4& l, const M4& r);

inline bool operator==(const V3& l, const V3& r);

inline bool operator==(const V4& l, const V4& r);

inline bool operator==(const M3& l, const M3& r);

inline bool operator==(const M4& l, const M4& r);

inline f32 V2Length(const V2& s);

inline f32 V3Length(const V3& s);

inline f32 V4Length(const V4& s);

inline V2 V2Norm(const V2& s);

inline V3 V3Norm(const V3& s, const f32 len);

inline V3 V3Norm(const V3& s);

inline V4 V4Norm(const V4& s);

inline V2 V2Clamp(const V2& x, const V2& min, const V2& max);

inline f32 V2Dot(const V2& l, const V2& r);

inline f32 V3Dot(const V3& l, const V3& r);

inline f32 V4Dot(const V4& l, const V4& r);

inline f32 V3Rad(const V3& l, const V3& r);

inline f32 V2Dist(const V2& l, const V2& r);

inline f32 V3Dist(const V3& l, const V3& r);

constexpr M4 M4TranslationFrom(const V3& tv);

constexpr M4 M4TranslationFrom(const f32 x, const f32 y, const f32 z);

inline M4 M4Translate(const M4& m, const V3& tv);

inline M3 M3Scale(const M3& m, const f32 s);

constexpr M4 M4ScaleFrom(const f32 s);

constexpr M4 M4ScaleFrom(const V3& v);

constexpr M4 M4ScaleFrom(f32 x, f32 y, f32 z);

inline M4 M4Scale(const M4& m, const f32 s);

inline M3 M3Scale(const M3& m, const V2& s);

inline M4 M4Scale(const M4& m, const V3& s);

inline M4 M4Pers(const f32 fov, const f32 asp, const f32 n, const f32 f);

inline M4 M4Ortho(const f32 l, const f32 r, const f32 b, const f32 t, const f32 n, const f32 f);

inline f32 V2Cross(const V2& l, const V2& r);

inline i64 IV2Cross(const IV2& l, const IV2& r);

inline V3 V3Cross(const V3& l, const V3& r);

inline M4 M4LookAt(const V3& R, const V3& U, const V3& D, const V3& P);

inline M4 M4RotFrom(const f32 th, const V3& ax);

inline M4 M4Rot(const M4& m, const f32 th, const V3& ax);

inline M4 M4RotXFrom(const f32 th);

inline M4 M4RotX(const M4& m, const f32 th);

inline M4 M4RotYFrom(const f32 th);

inline M4 M4RotY(const M4& m, const f32 th);

inline M4 M4RotZFrom(const f32 th);

inline M4 M4RotZ(const M4& m, const f32 th);

inline M4 M4RotFrom(const f32 x, const f32 y, const f32 z);

inline M4 M4LookAt(const V3& eyeV, const V3& centerV, const V3& upV);

inline Qt QtAxisAngle(const V3& axis, f32 th);

inline M4 QtRot(const Qt& q);

inline M4 QtRot2(const Qt& q);

inline Qt QtConj(const Qt& q);

inline Qt operator-(const Qt& l);

inline Qt operator*(const Qt& l, const Qt& r);

inline Qt operator*(const Qt& l, const V4& r);

inline Qt operator*=(Qt& l, const Qt& r);

inline Qt operator*=(Qt& l, const V4& r);

inline bool operator==(const Qt& a, const Qt& b);

inline Qt QtNorm(Qt a);

inline V2 normalize(const V2& v);

inline V3 normalize(const V3& v);

inline V4 normalize(const V4& v);

constexpr inline auto lerp(const auto& a, const auto& b, const auto& t);

inline Qt slerp(const Qt& q1, const Qt& q2, f32 t);

template<typename T>
constexpr T
bezier(
    const T& p0,
    const T& p1,
    const T& p2, const std::floating_point auto t);

template<typename T>
constexpr T
bezier(
    const T& p0,
    const T& p1,
    const T& p2,
    const T& p3, const std::floating_point auto t);

inline M4 transformation(const V3& translation, const Qt& rot, const V3& scale);

inline M4 transformation(const V3& translation, const V3& scale);

} /* namespace adt::math */

namespace adt::print
{

inline isize formatToContext(Context ctx, FormatArgs, const math::V2& x);
inline isize formatToContext(Context ctx, FormatArgs, const math::V3& x);
inline isize formatToContext(Context ctx, FormatArgs, const math::V4& x);
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const math::IV4& x);
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const math::IV4u16& x);
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const math::Qt& x);
inline isize formatToContext(Context ctx, FormatArgs, const math::M2& x);
inline isize formatToContext(Context ctx, FormatArgs, const math::M3& x);
inline isize formatToContext(Context ctx, FormatArgs, const math::M4& x);

} /* namespace adt::math */

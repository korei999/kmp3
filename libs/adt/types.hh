#pragma once

#include <cstdio>
#include <cstdlib>

#ifdef ADT_STD_TYPES
    #include <cstdint>
    #include <limits>
    #include <cstddef>
#endif

#if __has_include(<windows.h>)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #include <windows.h>
    #include <winuser.h>
#endif

namespace adt
{

#ifdef ADT_STD_TYPES

using i8 = int8_t;
using u8 = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;
using pdiff = ptrdiff_t;
using isize = i64;
using usize = size_t;

constexpr isize NPOS = -1L;
constexpr u16 NPOS8 = std::numeric_limits<u8>::max();
constexpr u16 NPOS16 = std::numeric_limits<u16>::max();
constexpr u32 NPOS32 = std::numeric_limits<u32>::max();
constexpr u64 NPOS64 = std::numeric_limits<u64>::max();

#else

using i8 = signed char;
using u8 = unsigned char;
using i16 = signed short;
using u16 = unsigned short;
using i32 = signed int;
using u32 = unsigned int;
using i64 = signed long long;
using u64 = unsigned long long;
using pdiff = long long;
using isize = long long;
using usize = unsigned long long;

constexpr int NPOS = -1;
constexpr u16 NPOS8 = u8(-1);
constexpr u16 NPOS16 = u16(-1);
constexpr u32 NPOS32 = u32(-1U);
constexpr u64 NPOS64 = u64(-1ULL);

#endif

using f32 = float;
using f64 = double;

using null = decltype(nullptr);

struct InitFlag {};
constexpr InitFlag INIT {};

#define ADT_WARN_INIT [[deprecated("warning: should be initialized with (INIT)")]]
#define ADT_WARN_DONT_USE [[deprecated("warning: don't use!")]]
#define ADT_WARN_IMPOSSIBLE_OPERATION [[deprecated("warning: imposibble operation")]]

#if defined __clang__ || __GNUC__
    #define ADT_NO_UB __attribute__((no_sanitize("undefined")))
    #define ADT_LOGS_FILE __FILE_NAME__
    #define ADT_FUNC_SIG __PRETTY_FUNCTION__
#else
    #define ADT_NO_UB
    #define ADT_LOGS_FILE __FILE__
    #define ADT_FUNC_SIG __FUNCSIG__
#endif

#if defined __clang__ || __GNUC__
    #define ADT_ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined _WIN32
    #define ADT_ALWAYS_INLINE inline __forceinline
#else
    #define ADT_ALWAYS_INLINE inline
#endif

#if defined _WIN32
    #define ADT_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
    #define ADT_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

template<typename METHOD_T>
[[nodiscard]] constexpr void*
methodPointerNonVirtual(METHOD_T ptr)
{
    return *reinterpret_cast<void**>(&ptr);
}

template<typename MemFn> struct PfnFromMethod;

/* non-const methods */
template<typename R, typename C, typename ...ARGS>
struct PfnFromMethod<R (C::*)(ARGS...)>
{
    using type = R (*)(C*, ARGS...);
};

/* const methods */
template<typename R, typename C, typename ...ARGS>
struct PfnFromMethod<R (C::*)(ARGS...) const>
{
    using type = R (*)(const C*, ARGS...);
};

template<typename MemFn>
using PfnFromMethodType = typename PfnFromMethod<MemFn>::type;

template<typename ...Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...; /* Inherit the call operators of all bases */
};

struct Empty { };

inline constexpr bool isPowerOf2(usize x) { return (x & (x - 1)) == 0; }

inline constexpr i8
nextPowerOf2(i8 x)
{
    --x;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;

    return ++x;
}

inline constexpr i16
nextPowerOf2(i16 x)
{
    --x;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;

    return ++x;
}

inline constexpr i32
nextPowerOf2(i32 x)
{
    --x;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

    return ++x;
}

inline constexpr isize
nextPowerOf2(isize x)
{
    --x;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;

    return ++x;
}

} /* namespace adt */

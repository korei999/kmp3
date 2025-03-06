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
using ssize = i64;
using usize = size_t;

constexpr ssize NPOS = -1L;
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
using ssize = long long;
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
#else
    #define ADT_NO_UB
    #define ADT_LOGS_FILE __FILE__
#endif

#if defined _WIN32
    #define ADT_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
    #define ADT_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

template<typename METHOD_T>
[[nodiscard]] constexpr void*
methodPointer(METHOD_T ptr)
{
    return *reinterpret_cast<void**>(&ptr);
}

template<typename ...Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...; /* Inherit the call operators of all bases */
};

template<typename ...Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

/* TODO: windows messagebox? */
inline void
assertionFailed(const char* cnd, const char* msg, const char* file, int line, const char* func)
{
    char aBuff[256] {};
    snprintf(aBuff, sizeof(aBuff) - 1, "[%s, %d: %s()] assertion( %s ) failed.\n(msg) '%s'\n", file, line, func, cnd, msg);

#if __has_include(<windows.h>)
    MessageBoxA(
        nullptr,
        aBuff,
        "Assertion failed",
        MB_ICONWARNING | MB_OK | MB_DEFBUTTON2
    );
#else
    fputs(aBuff, stderr);
    fflush(stderr);
#endif

#ifndef NDEBUG
    abort();
#endif
}

#ifndef NDEBUG
    #define ADT_ASSERT(CND, ...)                                                                                       \
        do                                                                                                             \
        {                                                                                                              \
            if (!static_cast<bool>(CND))                                                                               \
            {                                                                                                          \
                char aMsgBuff[128] {};                                                                                 \
                snprintf(aMsgBuff, sizeof(aMsgBuff) - 1, __VA_ARGS__);                                                 \
                adt::assertionFailed(#CND, aMsgBuff, ADT_LOGS_FILE, __LINE__, __func__);                               \
            }                                                                                                          \
        } while (0)
#else
    #define ADT_ASSERT(...) (void)0
#endif

#ifndef ADT_DISABLE_ASSERT_ALWAYS
    #define ADT_ASSERT_ALWAYS(CND, ...)                                                                                \
        do                                                                                                             \
        {                                                                                                              \
            if (!static_cast<bool>(CND))                                                                               \
            {                                                                                                          \
                char aMsgBuff[128] {};                                                                                 \
                snprintf(aMsgBuff, sizeof(aMsgBuff) - 1, __VA_ARGS__);                                                 \
                adt::assertionFailed(#CND, aMsgBuff, ADT_LOGS_FILE, __LINE__, __func__);                               \
            }                                                                                                          \
        } while (0)
#endif

} /* namespace adt */

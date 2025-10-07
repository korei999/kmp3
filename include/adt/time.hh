#pragma once

#include "types.hh"

#if __has_include(<unistd.h>)
    #include <unistd.h>
    #include <ctime>
#elif _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #undef near
    #undef far
    #undef NEAR
    #undef FAR
    #undef min
    #undef max
    #undef MIN
    #undef MAX
    #include <sysinfoapi.h>
#endif

/* Usage:
 *
 * time::Type prevTime = time::now();
 * ...
 * ...
 * auto nextTime = time::now();
 * if (time::diff(nextTime, prevTime) >= time::MSEC * 500)
 * {
 *     doThings();
 *     prevTime = nextTime;
 * }
 *
 ************************************/

namespace adt::time
{

using Type = isize;

static constexpr Type USEC = 1;
static constexpr Type MSEC = 1'000;
static constexpr Type SEC = 1'000'000;
static constexpr Type MIN = SEC * 60;
static constexpr Type HOUR = MIN * 60;
static constexpr Type DAY = HOUR * 24;
static constexpr Type WEEK = DAY * 7;
static constexpr Type YEAR = DAY * 365;

[[nodiscard]] inline Type
now() noexcept
{
#ifdef _MSC_VER

    LARGE_INTEGER ret;
    QueryPerformanceCounter(&ret);
    return ret.QuadPart;

#elif __has_include(<unistd.h>)

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((Type)ts.tv_sec * 1000000000ll) + (Type)ts.tv_nsec;

#endif
}

[[nodiscard]] inline Type
frequency() noexcept
{
#ifdef _MSC_VER

    static const LARGE_INTEGER s_freq = [] {
        LARGE_INTEGER t;
        QueryPerformanceFrequency(&t);
        return t;
    }();
    return s_freq.QuadPart;

#elif __has_include(<unistd.h>)

    return 1000000000ll;

#endif
}

[[nodiscard]] inline Type
diff(Type time, Type startTime) noexcept /* Unified to microseconds. */
{
    const Type diff = time - startTime;

#ifdef _MSC_VER

    return (diff * 1000000ll) / frequency();

#elif __has_include(<unistd.h>)

    return diff / 1000ll;

#endif
}

[[nodiscard]] inline f64
diffSec(Type time, Type startTime) noexcept
{
    return (f64)(time - startTime) / (f64)frequency();
}

[[nodiscard]] inline f64
diffMSec(Type time, Type startTime) noexcept
{
    return (f64)(time - startTime) * (1000.0 / (f64)frequency());
}

[[nodiscard]] inline Type
nowUS()
{
#if __has_include(<unistd.h>)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    time_t micros = ts.tv_sec * 1'000'000;
    micros += ts.tv_nsec / 1'000;

    return micros;

#elif _WIN32
    LARGE_INTEGER count;
    static const LARGE_INTEGER s_freq = []
    {
        LARGE_INTEGER t;
        QueryPerformanceFrequency(&t);
        return t;
    }();
    QueryPerformanceCounter(&count);

    return (count.QuadPart * 1'000'000) / s_freq.QuadPart;
#endif
}

[[nodiscard]] inline f64
nowMS()
{
    return static_cast<f64>(nowUS()) / 1000.0;
}

[[nodiscard]] inline f64
nowS()
{
    return static_cast<f64>(nowUS()) / 1'000'000.0;
}

} /* namespace adt::time */

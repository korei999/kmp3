#pragma once

#include "types.hh"

#if __has_include(<unistd.h>)
    #include <unistd.h>
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

#include <ctime>

namespace adt
{

struct Timer
{
    u64 m_start {};

    /* */

    Timer() = default;
    Timer(InitFlag) noexcept : m_start{getTime()} {}
    Timer(u64 time) noexcept : m_start{time} {}

    /* */

    void start() noexcept;
    void reset(u64 newTime) noexcept;

    f64 sElapsed() noexcept;
    f64 sElapsed(u64 time) noexcept;

    f64 msElapsed(u64 time) noexcept;
    f64 msElapsed() noexcept;

    u64 elapsed() noexcept;

    /* */

    static u64 frequency() noexcept;
    static u64 getTime() noexcept;
};

inline void
Timer::start() noexcept
{
    m_start = getTime();
}

[[nodiscard]] inline f64
Timer::sElapsed() noexcept
{
    return (f64)(getTime() - m_start) / (f64)frequency();
}

[[nodiscard]] inline f64
Timer::msElapsed() noexcept
{
    return sElapsed() * 1000.0;
}

[[nodiscard]] inline u64
Timer::elapsed() noexcept
{
    return getTime() - m_start;
}

inline void
Timer::reset(u64 newTime) noexcept
{
    m_start = newTime;
}

inline f64
Timer::sElapsed(u64 time) noexcept
{
    return (f64)(time - m_start) / (f64)frequency();
}

inline f64
Timer::msElapsed(u64 time) noexcept
{
    return sElapsed(time) * 1000.0;
}

inline u64
Timer::frequency() noexcept
{
#ifdef _MSC_VER

    static const LARGE_INTEGER s_freq = []
    {
        LARGE_INTEGER t;
        QueryPerformanceFrequency(&t);
        return t;
    }();
    return s_freq.QuadPart;

#elif __has_include(<unistd.h>)

    return 1000000000ull;

#endif
}

inline u64
Timer::getTime() noexcept
{
#ifdef _MSC_VER

    LARGE_INTEGER ret;
    QueryPerformanceCounter(&ret);
    return ret.QuadPart;

#elif __has_include(<unistd.h>)

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((u64)ts.tv_sec * 1000000000ull) + (u64)ts.tv_nsec;

#endif
}

} /* namespace adt */

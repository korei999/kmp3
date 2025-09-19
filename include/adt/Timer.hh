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
    static constexpr i64 SEC = 1'000'000; /* In microseconds. */
    static constexpr i64 MIN = SEC * 60;
    static constexpr i64 HOUR = MIN * 60;
    static constexpr i64 MS = SEC / 1000;

    /* */

    i64 m_startTime {};

    /* */

    Timer() = default;
    Timer(InitFlag) noexcept : m_startTime{getTime()} {}
    Timer(i64 time) noexcept : m_startTime{time} {}

    /* */

    i64 value() const noexcept { return m_startTime; }

    void reset() noexcept;
    void reset(i64 newTime) noexcept;

    f64 sElapsed() noexcept;
    f64 sElapsed(i64 time) noexcept;

    f64 msElapsed(i64 time) noexcept;
    f64 msElapsed() noexcept;

    i64 uElapsed() noexcept;
    i64 uElapsed(i64) noexcept;

    i64 elapsed() noexcept;

    /* */

    static i64 frequency() noexcept;
    static i64 getTime() noexcept;
};

inline void
Timer::reset() noexcept
{
    m_startTime = getTime();
}

[[nodiscard]] inline f64
Timer::sElapsed() noexcept
{
    return (f64)(getTime() - m_startTime) / (f64)frequency();
}

[[nodiscard]] inline f64
Timer::msElapsed() noexcept
{
    const i64 diff = getTime() - m_startTime;
    return ((f64)(diff) * 1000.0) / (f64)frequency();
}

[[nodiscard]] inline i64
Timer::uElapsed() noexcept
{
    return uElapsed(getTime());
}

[[nodiscard]] inline i64
Timer::uElapsed(i64 time) noexcept
{
    const i64 diff = time - m_startTime;

#ifdef _MSC_VER

    return diff * 1000000ll / frequency();

#elif __has_include(<unistd.h>)

    return diff / 1000ll;

#endif
}

[[nodiscard]] inline i64
Timer::elapsed() noexcept
{
    return getTime() - m_startTime;
}

inline void
Timer::reset(i64 newTime) noexcept
{
    m_startTime = newTime;
}

inline f64
Timer::sElapsed(i64 time) noexcept
{
    return (f64)(time - m_startTime) / (f64)frequency();
}

inline f64
Timer::msElapsed(i64 time) noexcept
{
    const i64 diff = time - m_startTime;
    return ((f64)(diff) * 1000.0) / (f64)frequency();
}

inline i64
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

    return 1000000000ll;

#endif
}

inline i64
Timer::getTime() noexcept
{
#ifdef _MSC_VER

    LARGE_INTEGER ret;
    QueryPerformanceCounter(&ret);
    return ret.QuadPart;

#elif __has_include(<unistd.h>)

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((i64)ts.tv_sec * 1000000000ll) + (i64)ts.tv_nsec;

#endif
}

} /* namespace adt */

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

namespace adt::time
{

static constexpr i64 USEC = 1;
static constexpr i64 MSEC = 1'000;
static constexpr i64 SEC = 1'000'000;
static constexpr i64 MIN = SEC * 60;
static constexpr i64 HOUR = MIN * 60;
static constexpr i64 DAY = HOUR * 24;
static constexpr i64 WEEK = DAY * 7;
static constexpr i64 YEAR = DAY * 365;

struct Clock /* In microseconds. */
{
    /* */

    i64 m_startTime {};

    /* */

    Clock() = default;
    explicit Clock(InitFlag) noexcept : m_startTime{getTime()} {}
    explicit Clock(i64 time) noexcept : m_startTime{time} {}

    /* */

    i64 value() const noexcept { return m_startTime; }

    void reset() noexcept;
    void reset(i64 newTime) noexcept;

    i64 elapsed() noexcept;
    i64 elapsed(i64) noexcept;

    f64 elapsedSec() noexcept;
    f64 elapsedSec(i64 time) noexcept;

    f64 elapsedMSec(i64 time) noexcept;
    f64 elapsedMSec() noexcept;

    /* */

    static i64 frequency() noexcept;
    static i64 getTime() noexcept;
};

inline void
Clock::reset() noexcept
{
    m_startTime = getTime();
}

[[nodiscard]] inline f64
Clock::elapsedSec() noexcept
{
    return (f64)(getTime() - m_startTime) / (f64)frequency();
}

[[nodiscard]] inline f64
Clock::elapsedMSec() noexcept
{
    const i64 diff = getTime() - m_startTime;
    return ((f64)(diff) * 1000.0) / (f64)frequency();
}

[[nodiscard]] inline i64
Clock::elapsed() noexcept
{
    return elapsed(getTime());
}

[[nodiscard]] inline i64
Clock::elapsed(i64 time) noexcept
{
    const i64 diff = time - m_startTime;

#ifdef _MSC_VER

    return diff * 1000000ll / frequency();

#elif __has_include(<unistd.h>)

    return diff / 1000ll;

#endif
}

inline void
Clock::reset(i64 newTime) noexcept
{
    m_startTime = newTime;
}

inline f64
Clock::elapsedSec(i64 time) noexcept
{
    return (f64)(time - m_startTime) / (f64)frequency();
}

inline f64
Clock::elapsedMSec(i64 time) noexcept
{
    const i64 diff = time - m_startTime;
    return ((f64)(diff) * 1000.0) / (f64)frequency();
}

inline i64
Clock::frequency() noexcept
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
Clock::getTime() noexcept
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

[[nodiscard]] inline adt::isize
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

[[nodiscard]] inline adt::f64
nowMS()
{
    return static_cast<adt::f64>(nowUS()) / 1000.0;
}

[[nodiscard]] inline adt::f64
nowS()
{
    return static_cast<adt::f64>(nowUS()) / 1'000'000.0;
}

} /* namespace adt::time */

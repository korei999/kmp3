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

namespace adt::time
{

constexpr isize US = 1'000'000;
constexpr isize MS = 1'000;

[[nodiscard]] inline isize
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

inline constexpr void
addNSToTimespec(timespec* const pTs, const isize nsec)
{
    constexpr isize nSecMax = 1000000000;
    /* overflow check */
    if (pTs->tv_nsec + nsec >= nSecMax)
    {
        pTs->tv_sec += 1;
        pTs->tv_nsec = (pTs->tv_nsec + nsec) - nSecMax;
    }
    else pTs->tv_nsec += nsec;
}
} /* namespace adt::time */

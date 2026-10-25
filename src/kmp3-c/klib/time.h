#pragma once

#include "common.h"

#if defined __x86_64
    #include <x86intrin.h>
#elif defined _WIN64
#else
    #error "no __rdtcs()"
#endif

typedef ssize_t k_time_Type;
typedef uint64_t k_time_Clocks;

#define K_TIME_USEC 1ll
#define K_TIME_MSEC 1000ll
#define K_TIME_SEC 1000000ll
#define K_TIME_MIN (K_TIME_SEC * 60ll)
#define K_TIME_HOUR (K_TIME_MIN * 60ll)
#define K_TIME_DAY (K_TIME_HOUR * 24ll)
#define K_TIME_WEEK (K_TIME_DAY * 7ll)
#define K_TIME_YEAR (K_TIME_WEEK * 365ll)

k_time_Type k_time_now(void);
k_time_Type k_time_frequency(void);
static inline k_time_Type k_time_diff(k_time_Type timeNow, k_time_Type startTime); /* Microseconds. */
static inline double k_time_diffSec(k_time_Type time, k_time_Type startTime);
static inline double k_time_diffMSec(k_time_Type time, k_time_Type startTime);

static inline k_time_Clocks k_time_stamp(void);

static inline k_time_Type
k_time_diff(k_time_Type endTime, k_time_Type startTime)
{
    const k_time_Type diff = endTime - startTime;

#if defined _WIN32

    return (diff * 1000000ll) / k_time_frequency();

#elif defined __unix__

    return diff / 1000ll;

#endif
}

static inline double
k_time_diffSec(k_time_Type endTime, k_time_Type startTime)
{
    return (double)(endTime - startTime) / (double)k_time_frequency();
}

static inline double
k_time_diffMSec(k_time_Type endTime, k_time_Type startTime) /* Millisecond. */
{
    return (double)(endTime - startTime) * (1000.0 / (double)k_time_frequency());
}

static inline k_time_Clocks
k_time_stamp(void)
{
#ifdef __x86_64

    return __rdtsc();

#else

    return (k_time_Clocks)k_time_now();

#endif
}

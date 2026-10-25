#include "time.h"

#if defined _WIN32

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

#elif defined __unix__

    #include <unistd.h>
    #include <time.h>

#endif

k_time_Type
k_time_now(void)
{
#if defined _WIN32

    LARGE_INTEGER ret;
    QueryPerformanceCounter(&ret);
    return ret.QuadPart;

#elif defined __unix__

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((k_time_Type)ts.tv_sec * 1000000000ll) + (k_time_Type)ts.tv_nsec;

#endif
}

k_time_Type
k_time_frequency(void)
{
#if defined _WIN32

    static k_time_Type s_freq;
    if (s_freq == 0)
    {
        LARGE_INTEGER t;
        QueryPerformanceFrequency(&t);
        s_freq = t.QuadPart;
    }
    return s_freq;

#elif defined __unix__

    return 1000000000ll;

#endif
}

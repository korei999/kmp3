#pragma once

#ifdef __linux__
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

#include "types.hh"

#include <ctime>
#include <cstring>
#include <cassert>

namespace adt
{
namespace utils
{

template<typename T>
constexpr void
swap(T* l, T* r)
{
    auto t0 = *l;
    auto t1 = *r;
    *l = t1;
    *r = t0;
}

constexpr void
toggle(auto* x)
{
    *x = !*x;
}

[[nodiscard]] constexpr auto&
max(const auto& l, const auto& r)
{
    return l > r ? l : r;
}

[[nodiscard]] constexpr auto&
min(const auto& l, const auto& r)
{
    return l < r ? l : r;
}

[[nodiscard]] constexpr u64
size(const auto& a)
{
    return sizeof(a) / sizeof(a[0]);
}

template<typename T>
[[nodiscard]] constexpr bool
odd(const T& a)
{
    return a & 1;
}

[[nodiscard]] constexpr bool
even(const auto& a)
{
    return !odd(a);
}

/* negative if l < r, positive if l > r, 0 if l == r */
template<typename T>
[[nodiscard]] constexpr s64
compare(const T& l, const T& r)
{
    return l - r;
}

template<typename T>
[[nodiscard]] constexpr s64
compareRev(const T& l, const T& r)
{
    return r - l;
}

[[nodiscard]] inline long
timeNowUS()
{
#ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    time_t micros = ts.tv_sec * 1000000000;
    micros += ts.tv_nsec;

    return micros;

#elif _WIN32
    LARGE_INTEGER count, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);

    return (count.QuadPart * 1000000000) / freq.QuadPart;
#endif
}

[[nodiscard]] inline f64
timeNowMS()
{
#ifdef __linux__
    return timeNowUS() / 1000000.0;

#elif _WIN32
    LARGE_INTEGER count, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);

    auto ret = (count.QuadPart * 1000000) / freq.QuadPart;
    return f64(ret) / 1000.0;
#endif
}

[[nodiscard]] inline f64
timeNowS()
{
    return timeNowMS() / 1000.0;
}

inline void
sleepMS(f64 ms)
{
#ifdef __linux__
    usleep(ms * 1000.0);
#elif _WIN32
    Sleep(ms);
#endif
}

constexpr void
addNSToTimespec(timespec* const pTs, const long nsec)
{
    constexpr long nsecMax = 1000000000;
    /* overflow check */
    if (pTs->tv_nsec + nsec >= nsecMax)
    {
        pTs->tv_sec += 1;
        pTs->tv_nsec = (pTs->tv_nsec + nsec) - nsecMax;
    }
    else pTs->tv_nsec += nsec;
}

template<typename T>
inline void
copy(T* pDest, T* pSrc, u64 size)
{
    assert(pDest != nullptr);
    assert(pSrc != nullptr);
    memcpy(pDest, pSrc, size * sizeof(T));
}

template<typename T>
constexpr void
fill(T* pData, T x, u64 size)
{
    for (u64 i = 0; i < size; ++i)
        pData[i] = x;
}

template<typename T>
[[nodiscard]] constexpr auto
clamp(const T& x, const T& _min, const T& _max)
{
    return max(_min, min(_max, x));
}

template<template<typename> typename CON_T, typename T>
[[nodiscard]] constexpr bool
empty(const CON_T<T>* s)
{
    return s->size == 0;
}

template<template<typename> typename CON_T, typename T>
[[nodiscard]] inline T&
searchMax(CON_T<T>* s)
{
    assert(!empty(s));

    auto _max = s->begin();
    for (auto it = ++s->begin(); it != s->end(); ++it)
        if (*it > *_max) _max = it;

    return *_max;
}

template<template<typename> typename CON_T, typename T>
[[nodiscard]] inline T&
searchMin(CON_T<T>* s)
{
    assert(!empty(s));

    auto _min = s->begin();
    for (auto it = ++s->begin(); it != s->end(); ++it)
        if (*it < *_min) _min = it;

    return *_min;
}

constexpr void
reverse(auto* a, const u32 size)
{
    for (u32 i = 0; i < size / 2; ++i)
        swap(&a[i], &a[size - 1 - i]);
}

constexpr void
reverse(auto* a)
{
    reverse(a->pData, a->size);
}

} /* namespace utils */
} /* namespace adt */

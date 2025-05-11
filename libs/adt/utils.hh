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

#include "PairDecl.hh"
#include "assert.hh"

#include <ctime>
#include <cstring>

namespace adt::utils
{

/* bit number starts from 0 */
inline constexpr ssize
setBit(ssize num, ssize bit, bool val)
{
    return (num & ~((ssize)1 << bit)) | ((ssize)val << bit);
}

template<typename T>
inline constexpr void
swap(T* l, T* r)
{
    T t = *r;
    *r = *l;
    *l = t;
}

inline constexpr void
toggle(auto* x)
{
    *x = !*x;
}

inline constexpr void
negate(auto* x)
{
    *x = -(*x);
}

template<typename T>
[[nodiscard]] inline const T&
max(const T& l, const T& r)
{
    return l > r ? l : r;
}

template<typename T>
[[nodiscard]] inline const T&
min(const T& l, const T& r)
{
    return l < r ? l : r;
}

template<typename T>
[[nodiscard]] inline Pair<T, T>
minMax(const T& l, const T& r)
{
    if (l > r) return {r, l};
    else return {l, r};
}

[[nodiscard]] inline constexpr ssize
size(const auto& a)
{
    return sizeof(a) / sizeof(a[0]);
}

template<typename T>
[[nodiscard]] inline constexpr bool
odd(const T& a)
{
    return a & 1;
}

[[nodiscard]] inline constexpr bool
even(const auto& a)
{
    return !odd(a);
}

template<typename T>
[[nodiscard]] inline constexpr ssize
compare(const T& l, const T& r)
{
    if (l == r) return 0;
    else if (l > r) return 1;
    else return -1;
}

template<typename T>
[[nodiscard]] inline constexpr ssize
compareRev(const T& l, const T& r)
{
    if (l == r) return 0;
    else if (l < r) return 1;
    else return -1;
}

[[nodiscard]] inline ssize
timeNowUS()
{
#ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    time_t micros = ts.tv_sec * 1'000'000;
    micros += ts.tv_nsec / 1'000;

    return micros;

#elif _WIN32
    LARGE_INTEGER count, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);

    return (count.QuadPart * 1'000'000) / freq.QuadPart;
#endif
}

[[nodiscard]] inline f64
timeNowMS()
{
    return static_cast<f64>(timeNowUS()) / 1000.0;
}

[[nodiscard]] inline f64
timeNowS()
{
    return static_cast<f64>(timeNowMS()) / 1000.0;
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

inline void
sleepS(f64 s)
{
#ifdef __linux__
    usleep(s * 1'000'000.0);
#elif _WIN32
    Sleep(s * 1000.0);
#endif
}

inline constexpr void
addNSToTimespec(timespec* const pTs, const ssize nsec)
{
    constexpr ssize nsecMax = 1000000000;
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
memCopy(T* pDest, const T* const pSrc, ssize size)
{
    ADT_ASSERT(pDest != nullptr && pSrc != nullptr, " ");
    memcpy(pDest, pSrc, size * sizeof(T));
}

/* typed memmove (don't mistake for std::move) */
template<typename T>
inline void
memMove(T* pDest, const T* const pSrc, ssize size)
{
    ADT_ASSERT(pDest != nullptr && pSrc != nullptr, " ");
    memmove(pDest, pSrc, size * sizeof(T));
}

template<typename T>
inline void
memSet(T* pDest, int byte, ssize size)
{
    ADT_ASSERT(pDest != nullptr, " ");
    memset(pDest, byte, size * sizeof(T));
}

template<typename T>
inline constexpr void
fill(T* pData, T x, ssize size)
{
    for (ssize i = 0; i < size; ++i)
        pData[i] = x;
}

template<typename T>
[[nodiscard]] inline constexpr auto
clamp(const T& x, const T& _min, const T& _max)
{
    return max(_min, min(_max, x));
}

template<template<typename> typename CON_T, typename T>
[[nodiscard]] inline T&
searchMax(CON_T<T>* s)
{
    ADT_ASSERT(!empty(s), " ");

    auto _max = s->begin();
    for (auto it = ++s->begin(); it != s->end(); ++it)
        if (*it > *_max) _max = it;

    return *_max;
}

template<template<typename> typename CON_T, typename T>
[[nodiscard]] inline T&
searchMin(CON_T<T>* s)
{
    ADT_ASSERT(!empty(s), " ");

    auto _min = s->begin();
    for (auto it = ++s->begin(); it != s->end(); ++it)
        if (*it < *_min) _min = it;

    return *_min;
}

inline constexpr void
reverse(auto* a, const ssize size)
{
    ADT_ASSERT(size > 0, " ");

    const ssize halfSize = size >> 1;
    for (ssize i = 0; i < halfSize; ++i)
        swap(&a[i], &a[size - 1 - i]);
}

inline constexpr void
reverse(auto* a)
{
    reverse(a->data(), a->size());
}

template<template<typename> typename CON_T, typename T, typename LAMBDA>
[[nodiscard]] inline ssize
search(const CON_T<T>& c, LAMBDA f)
{
    for (const auto& el : c)
        if (f(el)) return c.idx(&el);

    return NPOS;
}

template<typename T, typename B>
[[nodiscard]] inline ssize
binarySearch(const T& array, const B& x)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(array[0])>, std::remove_cvref_t<B>>);

    ADT_ASSERT(array.size() > 0, "");

    ssize lo = 0, hi = array.size();
    while (lo < hi)
    {
        const ssize mid = (lo + hi) >> 1;

        const bool less = (array[mid] < x);
        lo = less ? mid + 1 : lo;
        hi = less ? hi : mid;
    }

    if (lo < array.size() && array[lo] == x)
        return lo;

    return NPOS;
}

template<typename T> requires(std::is_integral_v<T>)
inline T
cycleForward(const T& idx, ssize size)
{
    return (idx + 1) % size;
}

template<typename T> requires(std::is_integral_v<T>)
inline T
cycleBackward(const T& idx, ssize size)
{
    return (idx + (size - 1)) % size;
}

} /* namespace adt::utils */

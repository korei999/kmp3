#pragma once

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

#include "Pair.hh"
#include "assert.hh"
#include "String.inc"

#include <ctime>
#include <cstring>
#include <utility>

namespace adt::utils
{

template<typename T, typename ...ARGS>
inline void
moveDestruct(T* pVal, ARGS&&... args)
{
    T tmp = T (std::forward<ARGS>(args)...);
    new(pVal) T (std::move(tmp));

    if constexpr (!std::is_trivially_destructible_v<T>)
        tmp.~T();
}

/* bit number starts from 0 */
inline constexpr isize
setBit(isize num, isize bit, bool val)
{
    return (num & ~((isize)1 << bit)) | ((isize)val << bit);
}

template<typename T>
inline constexpr void
swap(T* l, T* r)
{
    T t = std::move(*r);
    *r = std::move(*l);
    *l = std::move(t);
}

template<typename T>
[[nodiscard]] inline constexpr T
exchange(T* pObj, T& replaceObjWith)
{
    T ret = std::move(*pObj);
    *pObj = replaceObjWith;
    return ret;
}

template<typename T>
[[nodiscard]] inline constexpr T
exchange(T* pObj, T&& replaceObjWith)
{
    T ret = std::move(*pObj);
    *pObj = std::move(replaceObjWith);
    return ret;
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
[[nodiscard]] inline constexpr const T&
max(const T& l, const T& r)
{
    return l > r ? l : r;
}

template<typename T>
[[nodiscard]] inline constexpr const T&
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

[[nodiscard]] inline constexpr isize
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

[[nodiscard]] inline isize
compare(const StringView& l, const StringView& r)
{
    const isize len = utils::min(l.m_size, r.m_size);
    const isize res = ::strncmp(l.m_pData, r.m_pData, len);

    if (res == 0) return l.m_size > r.m_size;
    else return res;
}

[[nodiscard]] inline isize
compareRev(const StringView& l, const StringView& r)
{
    const isize len = utils::min(l.m_size, r.m_size);
    const isize res = ::strncmp(r.m_pData, l.m_pData, len);

    if (res == 0) return r.m_size > l.m_size;
    else return res;
}

template<typename T>
requires (!std::convertible_to<T, StringView>)
[[nodiscard]] inline constexpr isize
compare(const T& l, const T& r)
{
    if constexpr (std::is_integral_v<T>)
        return l - r;

    if (l == r) return 0;
    else if (l > r) return 1;
    else return -1;
}

template<typename T>
requires (!std::convertible_to<T, StringView>)
[[nodiscard]] inline constexpr isize
compareRev(const T& l, const T& r)
{
    if constexpr (std::is_integral_v<T>)
        return r - l;

    if (l == r) return 0;
    else if (l < r) return 1;
    else return -1;
}

template<typename T>
struct Comparator
{
    constexpr isize
    operator()(const T& l, const T& r) const noexcept
    {
        return compare(l, r);
    }
};

template<typename T>
struct ComparatorRev
{
    constexpr isize
    operator()(const T& l, const T& r) const noexcept
    {
        return compareRev(l, r);
    }
};

[[nodiscard]] inline isize
timeNowUS()
{
#if __has_include(<unistd.h>)
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
#if __has_include(<unistd.h>)
    usleep(ms * 1000.0);
#elif _WIN32
    Sleep(ms);
#endif
}

inline void
sleepS(f64 s)
{
#if __has_include(<unistd.h>)
    usleep(s * 1'000'000.0);
#elif _WIN32
    Sleep(s * 1000.0);
#endif
}

inline constexpr void
addNSToTimespec(timespec* const pTs, const isize nsec)
{
    constexpr isize nsecMax = 1000000000;
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
memCopy(T* pDest, const T* const pSrc, isize size)
{
    ADT_ASSERT(pDest != nullptr && pSrc != nullptr, " ");
    memcpy(pDest, pSrc, size * sizeof(T));
}

/* typed memmove (don't mistake for std::move) */
template<typename T>
inline void
memMove(T* pDest, const T* const pSrc, isize size)
{
    ADT_ASSERT(pDest != nullptr && pSrc != nullptr, " ");
    memmove(pDest, pSrc, size * sizeof(T));
}

template<typename T>
inline void
memSet(T* pDest, int byte, isize size)
{
    ADT_ASSERT(pDest != nullptr, " ");
    memset(pDest, byte, size * sizeof(T));
}

template<typename T>
[[nodiscard]] inline constexpr auto
clamp(const T& x, const T& _min, const T& _max)
{
    return max(_min, min(_max, x));
}

inline constexpr void
reverse(auto* a, const isize size)
{
    const isize halfSize = size >> 1;
    for (isize i = 0; i < halfSize; ++i)
        swap(&a[i], &a[size - 1 - i]);
}

inline constexpr void
reverse(auto* a)
{
    reverse(a->data(), a->size());
}

template<typename LAMBDA>
[[nodiscard]] inline isize
searchI(const auto& a, LAMBDA cl)
{
    isize i = 0;
    for (const auto& e : a)
    {
        if (cl(e)) return i;
        ++i;
    }

    return NPOS;
}

template<typename T, typename B>
[[nodiscard]] inline isize
binarySearchI(const T& array, const B& x)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(array[0])>, std::remove_cvref_t<B>>);

    ADT_ASSERT(array.size() > 0, "");

    isize lo = 0, hi = array.size();
    while (lo < hi)
    {
        const isize mid = (lo + hi) >> 1;

        const bool less = (array[mid] < x);
        lo = less ? mid + 1 : lo;
        hi = less ? hi : mid;
    }

    if (lo < array.size() && array[lo] == x)
        return lo;

    return NPOS;
}

template<typename T> requires(std::is_integral_v<T>)
[[nodiscard]] inline T
cycleForward(const T& idx, isize size)
{
    return (idx + 1) % size;
}

template<typename T> requires(std::is_integral_v<T>)
[[nodiscard]] inline T
cycleBackward(const T& idx, isize size)
{
    return (idx + (size - 1)) % size;
}

template<typename T> requires(std::is_integral_v<T>)
[[nodiscard]] inline T
cycleForwardPowerOf2(const T& i, isize size)
{
    ADT_ASSERT(isPowerOf2(size), "size: {}", size);

    return (i + 1) & (size - 1);
}

template<typename T> requires(std::is_integral_v<T>)
[[nodiscard]] inline T
cycleBackwardPowerOf2(const T& i, isize size)
{
    ADT_ASSERT(isPowerOf2(size), "size: {}", size);

    return (i - 1) & (size - 1);
}

} /* namespace adt::utils */

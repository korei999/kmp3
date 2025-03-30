#pragma once

#include "types.hh"
#include "IException.hh"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

namespace adt
{

inline constexpr usize align(usize x, usize to) { return ((x) + to - 1) & (~(to - 1)); }
inline constexpr usize align8(usize x) { return align(x, 8); }
inline constexpr bool isPowerOf2(usize x) { return (x & (x - 1)) == 0; }

constexpr ssize SIZE_MIN = 2;
constexpr ssize SIZE_1K = 1024;
constexpr ssize SIZE_8K = SIZE_1K * 8;
constexpr ssize SIZE_1M = SIZE_1K * SIZE_1K;
constexpr ssize SIZE_8M = SIZE_1M * 8;
constexpr ssize SIZE_1G = SIZE_1M * SIZE_1K;
constexpr ssize SIZE_8G = SIZE_1G * 8;

#define ADT_WARN_LEAK [[deprecated("warning: memory leak")]]

struct IAllocator
{
    template<typename T, typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    [[nodiscard]] constexpr T*
    alloc(ARGS&&... args)
    {
        auto* p = (T*)malloc(1, sizeof(T));
        new(p) T(std::forward<ARGS>(args)...);
        return p;
    }

    template<typename T>
    [[nodiscard]] constexpr T*
    mallocV(ssize mCount)
    {
        return reinterpret_cast<T*>(malloc(mCount, sizeof(T)));
    }

    template<typename T>
    [[nodiscard]] constexpr T*
    zallocV(ssize mCount)
    {
        return reinterpret_cast<T*>(zalloc(mCount, sizeof(T)));
    }

    template<typename T>
    [[nodiscard]] constexpr T*
    reallocV(T* ptr, ssize oldCount, ssize newCount)
    {
        return reinterpret_cast<T*>(realloc(ptr, oldCount, newCount, sizeof(T)));
    }

    [[nodiscard]] virtual constexpr void* malloc(usize mCount, usize mSize) noexcept(false) = 0;
    [[nodiscard]] virtual constexpr void* zalloc(usize mCount, usize mSize) noexcept(false) = 0;
    /* pass oldCount to simpilify memcpy range */
    [[nodiscard]] virtual constexpr void* realloc(void* p, usize oldCount, usize newCount, usize mSize) noexcept(false) = 0;
    virtual constexpr void free(void* ptr) noexcept = 0;
    virtual constexpr void freeAll() noexcept = 0;
};

/* NOTE: allocator can throw on malloc/zalloc/realloc */
/* TODO: get rid of exceptions in favor of nullptr.
 * Classes can receive react to nullptr by preserving their state or replace their state with preallocated stub. */
struct AllocException : public IException
{
    const char* m_ntsMsg {};

    /* */

    AllocException() = default;
    AllocException(const char* ntsMsg) : m_ntsMsg(ntsMsg) {}

    /* */

    virtual ~AllocException() = default;

    /* */

    virtual void
    printErrorMsg(FILE* fp) const override
    {
        char aBuff[128] {};
        snprintf(aBuff, sizeof(aBuff) - 1, "AllocException: '%s', errno: '%s'\n", m_ntsMsg, strerror(errno));
        fputs(aBuff, fp);
    }

    virtual const char* getMsg() const override { return m_ntsMsg; }
};

} /* namespace adt */

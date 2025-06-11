#pragma once

#include "types.hh"
#include "IException.hh"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>
#include <new> /* IWYU pragma: keep */

namespace adt
{

inline constexpr usize alignUp(usize x, usize to) { return ((x) + to - 1) & (~(to - 1)); }
inline constexpr usize alignUp8(usize x) { return alignUp(x, 8); }

constexpr isize SIZE_MIN = 2;
constexpr isize SIZE_1K = 1024;
constexpr isize SIZE_8K = SIZE_1K * 8;
constexpr isize SIZE_1M = SIZE_1K * SIZE_1K;
constexpr isize SIZE_8M = SIZE_1M * 8;
constexpr isize SIZE_1G = SIZE_1M * SIZE_1K;
constexpr isize SIZE_8G = SIZE_1G * 8;

#define ADT_WARN_LEAK [[deprecated("warning: memory leak")]]

struct IAllocator
{
    template<typename T, typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    [[nodiscard]] constexpr T*
    alloc(ARGS&&... args) noexcept(false) /* AllocException */
    {
        auto* p = (T*)malloc(1, sizeof(T));
        new(p) T(std::forward<ARGS>(args)...);
        return p;
    }

    template<typename T>
    [[nodiscard]] constexpr T*
    mallocV(isize mCount) noexcept(false) /* AllocException */
    {
        return static_cast<T*>(malloc(mCount, sizeof(T)));
    }

    template<typename T>
    [[nodiscard]] constexpr T*
    zallocV(isize mCount) noexcept(false) /* AllocException */
    {
        return static_cast<T*>(zalloc(mCount, sizeof(T)));
    }

    template<typename T>
    [[nodiscard]] constexpr T*
    reallocV(T* ptr, isize oldCount, isize newCount) noexcept(false) /* AllocException */
    {
        return static_cast<T*>(realloc(ptr, oldCount, newCount, sizeof(T)));
    }

    [[nodiscard]] virtual constexpr void* malloc(usize mCount, usize mSize) noexcept(false) = 0; /* AllocException */

    [[nodiscard]] virtual constexpr void* zalloc(usize mCount, usize mSize) noexcept(false) = 0; /* AllocException */

    /* pass oldCount to simpilify memcpy range */
    [[nodiscard]] virtual constexpr void* realloc(void* p, usize oldCount, usize newCount, usize mSize) noexcept(false) = 0; /* AllocException */

    virtual constexpr void free(void* ptr) noexcept = 0;
};

struct IArena : IAllocator
{
    virtual constexpr void freeAll() noexcept = 0;
};

/* NOTE: allocator can throw on malloc/zalloc/realloc */
struct AllocException : public IException
{
    StringFixed<128> m_sfMsg {};

    /* */

    AllocException() = default;
    AllocException(const StringView svMsg) : m_sfMsg {svMsg} {}

    /* */

    virtual ~AllocException() = default;

    /* */

    virtual void
    printErrorMsg(FILE* fp) const override
    {
        char aBuff[128] {};
        print::toSpan(aBuff, "AllocException: '{}', errno: '{}'\n", m_sfMsg, strerror(errno));
        fputs(aBuff, fp);
    }

    virtual StringView getMsg() const override { return m_sfMsg; }
};

} /* namespace adt */

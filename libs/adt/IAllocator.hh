#pragma once

#include "types.hh"
#include "IException.hh"

#include <cerrno>
#include <cstdio>
#include <cstring>

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

struct IAllocator
{
    [[nodiscard]] virtual constexpr void* malloc(usize mCount, usize mSize) noexcept(false) = 0;

    [[nodiscard]] virtual constexpr void* zalloc(usize mCount, usize mSize) noexcept(false) = 0;

    /* pass oldCount to simpilify memcpy range */
    [[nodiscard]] virtual constexpr void* realloc(void* p, usize oldCount, usize newCount, usize mSize) noexcept(false) = 0;

    virtual constexpr void free(void* ptr) noexcept = 0;

    virtual constexpr void freeAll() noexcept = 0;
};

/* NOTE: allocator can throw on malloc/zalloc/realloc */
/* TODO: get rid of exceptions in favor of nullptr.
 * Other classes can receive nullptr and preserve their state or replace their state with statically allocated stub. */
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
    logErrorMsg(FILE* fp) override
    {
        char aBuff[128] {};
        snprintf(aBuff, sizeof(aBuff) - 1, "AllocException: '%s', errno: '%s'\n", m_ntsMsg, strerror(errno));
        fputs(aBuff, fp);
    }

    virtual const char* getMsg() override { return m_ntsMsg; }
};

} /* namespace adt */

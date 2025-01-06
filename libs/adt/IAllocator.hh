#pragma once

#include "types.hh"

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
    [[nodiscard]] virtual constexpr void* malloc(usize mCount, usize mSize) = 0;
    [[nodiscard]] virtual constexpr void* zalloc(usize mCount, usize mSize) = 0;
    [[nodiscard]] virtual constexpr void* realloc(void* p, usize mCount, usize mSize) = 0;
    virtual constexpr void free(void* ptr) = 0;
    virtual constexpr void freeAll() = 0;
};

} /* namespace adt */

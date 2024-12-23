#pragma once

#include "types.hh"

namespace adt
{

constexpr u64 align(u64 x, u64 to) { return ((x) + to - 1) & (~(to - 1)); }
constexpr u64 align8(u64 x) { return align(x, 8); }
constexpr bool isPowerOf2(u64 x) { return (x & (x - 1)) == 0; }

constexpr u64
nextPowerOf2(u64 x)
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    ++x;

    return x;
}

constexpr u64 SIZE_MIN = 2UL;
constexpr u64 SIZE_1K = 1024UL;
constexpr u64 SIZE_8K = 8UL * SIZE_1K;
constexpr u64 SIZE_1M = SIZE_1K * SIZE_1K; 
constexpr u64 SIZE_8M = 8UL * SIZE_1M; 
constexpr u64 SIZE_1G = SIZE_1M * SIZE_1K; 
constexpr u64 SIZE_8G = SIZE_1G * SIZE_1K;

struct IAllocator
{
    [[nodiscard]] virtual constexpr void* malloc(u64 mCount, u64 mSize) = 0;
    [[nodiscard]] virtual constexpr void* zalloc(u64 mCount, u64 mSize) = 0;
    [[nodiscard]] virtual constexpr void* realloc(void* p, u64 mCount, u64 mSize) = 0;
    virtual constexpr void free(void* ptr) = 0;
    virtual constexpr void freeAll() = 0;
};

} /* namespace adt */

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

struct IAllocator;

/* at least alloc() and free() or freeAll() must be supported */
struct AllocatorVTable
{
    void* (* alloc)(IAllocator* s, u64 mCount, u64 mSize);
    void* (* zalloc)(IAllocator* s, u64 mCount, u64 mSize);
    void* (* realloc)(IAllocator* s, void* p, u64 mCount, u64 mSize); /* must alloc if p == nullptr */
    void (* free)(IAllocator* s, void* p);
    void (* freeAll)(IAllocator* s);
};

struct IAllocator
{
    const AllocatorVTable* pVTable {};

    /* */

    [[nodiscard]] ADT_NO_UB constexpr void* alloc(u64 mCount, u64 mSize) { return pVTable->alloc(this, mCount, mSize); }
    [[nodiscard]] ADT_NO_UB constexpr void* zalloc(u64 mCount, u64 mSize) { return pVTable->zalloc(this, mCount, mSize); }
    [[nodiscard]] ADT_NO_UB constexpr void* realloc(void* p, u64 mCount, u64 mSize) { return pVTable->realloc(this, p, mCount, mSize); }
    ADT_NO_UB constexpr void free(void* ptr) { pVTable->free(this, ptr); }
    ADT_NO_UB constexpr void freeAll() { pVTable->freeAll(this); }
};

template<typename ALLOC_T>
constexpr AllocatorVTable
AllocatorVTableGenerate()
{
    return AllocatorVTable {
        .alloc = decltype(AllocatorVTable::alloc)(methodPointer(&ALLOC_T::alloc)),
        .zalloc = decltype(AllocatorVTable::zalloc)(methodPointer(&ALLOC_T::zalloc)),
        .realloc = decltype(AllocatorVTable::realloc)(methodPointer(&ALLOC_T::realloc)),
        .free = decltype(AllocatorVTable::free)(methodPointer(&ALLOC_T::free)),
        .freeAll = decltype(AllocatorVTable::freeAll)(methodPointer(&ALLOC_T::freeAll)),
    };
}

} /* namespace adt */

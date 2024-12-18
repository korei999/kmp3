#pragma once

#include "IAllocator.hh"
#include "utils.hh"

#include <cassert>
#include <cstring>

namespace adt
{

struct FixedAllocator
{
    IAllocator super {};
    u8* pMemBuffer = nullptr;
    u64 size = 0;
    u64 cap = 0;
    void* pLastAlloc = nullptr;

    constexpr FixedAllocator() = default;
    constexpr FixedAllocator(void* pMemory, u64 capacity);

    [[nodiscard]] constexpr void* alloc(u64 mCount, u64 mSize);
    [[nodiscard]] constexpr void* zalloc(u64 mCount, u64 mSize);
    [[nodiscard]] constexpr void* realloc(void* ptr, u64 mCount, u64 mSize);
    constexpr void free(void* ptr);
    constexpr void freeAll();
    constexpr void reset();
};

constexpr void*
FixedAllocator::alloc(u64 mCount, u64 mSize)
{
    u64 aligned = align8(mCount * mSize);
    void* ret = &this->pMemBuffer[this->size];
    this->size += aligned;
    this->pLastAlloc = ret;

    assert(this->size < this->cap && "Out of memory");

    return ret;
}

constexpr void*
FixedAllocator::zalloc(u64 mCount, u64 mSize)
{
    auto* p = this->alloc(mCount, mSize);
    memset(p, 0, mCount * mSize);
    return p;
}

constexpr void*
FixedAllocator::realloc(void* p, u64 mCount, u64 mSize)
{
    if (!p) return this->alloc(mCount, mSize);

    void* ret = nullptr;
    u64 aligned = align8(mCount * mSize);

    if (p == this->pLastAlloc)
    {
        this->size -= (u8*)&this->pMemBuffer[this->size] - (u8*)p;
        this->size += aligned;

        return p;
    }
    else
    {
        ret = &this->pMemBuffer[this->size];
        this->pLastAlloc = ret;
        u64 nBytesUntilEndOfBlock = this->cap - this->size;
        u64 nBytesToCopy = utils::min(aligned, nBytesUntilEndOfBlock);
        memcpy(ret, p, nBytesToCopy);
        this->size += aligned;
    }

    return ret;
}

constexpr void
FixedAllocator::free(void*)
{
    //
}

constexpr void
FixedAllocator::freeAll()
{
    //
}

constexpr void
FixedAllocator::reset()
{
    this->size = 0;
}

inline const AllocatorVTable inl_FixedAllocatorVTable = AllocatorVTableGenerate<FixedAllocator>();

constexpr FixedAllocator::FixedAllocator(void* pMemory, u64 capacity)
    : super {&inl_FixedAllocatorVTable},
      pMemBuffer((u8*)pMemory),
      cap(capacity) {}

} /* namespace adt */

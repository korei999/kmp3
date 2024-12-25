#pragma once

#include "IAllocator.hh"
#include "utils.hh"

#include <cassert>
#include <cstring>

namespace adt
{

struct FixedAllocator : IAllocator
{
    u8* pMemBuffer = nullptr;
    u64 size = 0;
    u64 cap = 0;
    void* pLastAlloc = nullptr;
    
    /* */

    constexpr FixedAllocator() = default;
    constexpr FixedAllocator(u8* pMemory, u64 capacity);

    /* */

    [[nodiscard]] virtual constexpr void* malloc(u64 mCount, u64 mSize) override final;
    [[nodiscard]] virtual constexpr void* zalloc(u64 mCount, u64 mSize) override final;
    [[nodiscard]] virtual constexpr void* realloc(void* ptr, u64 mCount, u64 mSize) override final;
    constexpr virtual void free(void* ptr) override final;
    constexpr virtual void freeAll() override final;
    constexpr void reset();
};

constexpr void*
FixedAllocator::malloc(u64 mCount, u64 mSize)
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
    auto* p = this->malloc(mCount, mSize);
    memset(p, 0, mCount * mSize);
    return p;
}

constexpr void*
FixedAllocator::realloc(void* p, u64 mCount, u64 mSize)
{
    if (!p) return this->malloc(mCount, mSize);

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

constexpr FixedAllocator::FixedAllocator(u8* pMemory, u64 capacity)
    : pMemBuffer(pMemory),
      cap(capacity) {}

} /* namespace adt */

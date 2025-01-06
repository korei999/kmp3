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
    usize size = 0;
    usize cap = 0;
    void* pLastAlloc = nullptr;
    
    /* */

    constexpr FixedAllocator() = default;
    constexpr FixedAllocator(u8* pMemory, usize capacity);

    /* */

    [[nodiscard]] virtual constexpr void* malloc(usize mCount, usize mSize) override final;
    [[nodiscard]] virtual constexpr void* zalloc(usize mCount, usize mSize) override final;
    [[nodiscard]] virtual constexpr void* realloc(void* ptr, usize mCount, usize mSize) override final;
    constexpr virtual void free(void* ptr) override final;
    constexpr virtual void freeAll() override final;
    constexpr void reset();
};

constexpr void*
FixedAllocator::malloc(usize mCount, usize mSize)
{
    usize aligned = align8(mCount * mSize);
    void* ret = &this->pMemBuffer[this->size];
    this->size += aligned;
    this->pLastAlloc = ret;

    assert(this->size < this->cap && "Out of memory");

    return ret;
}

constexpr void*
FixedAllocator::zalloc(usize mCount, usize mSize)
{
    auto* p = this->malloc(mCount, mSize);
    memset(p, 0, mCount * mSize);
    return p;
}

constexpr void*
FixedAllocator::realloc(void* p, usize mCount, usize mSize)
{
    if (!p) return this->malloc(mCount, mSize);

    void* ret = nullptr;
    usize aligned = align8(mCount * mSize);

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
        usize nBytesUntilEndOfBlock = this->cap - this->size;
        usize nBytesToCopy = utils::min(aligned, nBytesUntilEndOfBlock);
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

constexpr FixedAllocator::FixedAllocator(u8* pMemory, usize capacity)
    : pMemBuffer(pMemory),
      cap(capacity) {}

} /* namespace adt */

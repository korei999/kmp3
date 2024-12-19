#pragma once

#include <threads.h>

#include "Arena.hh"

namespace adt
{

struct MutexArena
{
    Arena arena;
    mtx_t mtx;

    MutexArena() = default;
    MutexArena(u32 blockCap);

    [[nodiscard]] void* alloc(u64 mCount, u64 mSize);
    [[nodiscard]] void* zalloc(u64 mCount, u64 mSize);
    [[nodiscard]] void* realloc(void* ptr, u64 mCount, u64 mSize);
    void free(void* ptr);
    void freeAll();
};

inline void*
MutexArena::alloc(u64 mCount, u64 mSize)
{
    mtx_lock(&this->mtx);
    auto* r = this->arena.zalloc(mCount, mSize);
    mtx_unlock(&this->mtx);

    return r;
}

inline void*
MutexArena::zalloc(u64 mCount, u64 mSize)
{
    mtx_lock(&this->mtx);
    auto* r = this->arena.zalloc(mCount, mSize);
    mtx_unlock(&this->mtx);

    return r;
}

inline void*
MutexArena::realloc(void* p, u64 mCount, u64 mSize)
{
    mtx_lock(&this->mtx);
    auto* r = this->arena.realloc(p, mCount, mSize);
    mtx_unlock(&this->mtx);

    return r;
}

inline void
MutexArena::free(void*)
{
    /* noop */
}

inline void
MutexArena::freeAll()
{
    this->arena.freeAll();
    mtx_destroy(&this->mtx);
}

inline const AllocatorVTable inl_AtomicArenaAllocatorVTable = AllocatorVTableGenerate<MutexArena>();

inline
MutexArena::MutexArena(u32 blockCap)
    : arena(blockCap)
{
    arena.super = {&inl_AtomicArenaAllocatorVTable};
    mtx_init(&mtx, mtx_plain);
}

} /* namespace adt */

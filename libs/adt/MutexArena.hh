#pragma once

#include <threads.h>

#include "Arena.hh"

namespace adt
{

struct MutexArena
{
    Arena base {};

    /* */

    mtx_t m_mtx {};

    /* */

    MutexArena() = default;
    MutexArena(u32 blockCap);

    /* */

    [[nodiscard]] void* alloc(u64 mCount, u64 mSize);
    [[nodiscard]] void* zalloc(u64 mCount, u64 mSize);
    [[nodiscard]] void* realloc(void* ptr, u64 mCount, u64 mSize);
    void free(void* ptr);
    void freeAll();
};

inline void*
MutexArena::alloc(u64 mCount, u64 mSize)
{
    mtx_lock(&m_mtx);
    auto* r = base.zalloc(mCount, mSize);
    mtx_unlock(&m_mtx);

    return r;
}

inline void*
MutexArena::zalloc(u64 mCount, u64 mSize)
{
    mtx_lock(&m_mtx);
    auto* r = base.zalloc(mCount, mSize);
    mtx_unlock(&m_mtx);

    return r;
}

inline void*
MutexArena::realloc(void* p, u64 mCount, u64 mSize)
{
    mtx_lock(&m_mtx);
    auto* r = base.realloc(p, mCount, mSize);
    mtx_unlock(&m_mtx);

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
    base.freeAll();
    mtx_destroy(&m_mtx);
}

inline const AllocatorVTable inl_AtomicArenaAllocatorVTable = AllocatorVTableGenerate<MutexArena>();

inline
MutexArena::MutexArena(u32 blockCap)
    : base(blockCap)
{
    base.super = {&inl_AtomicArenaAllocatorVTable};
    mtx_init(&m_mtx, mtx_plain);
}

} /* namespace adt */

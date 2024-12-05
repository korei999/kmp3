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
};

inline void* MutexArenaAlloc(MutexArena* s, u64 mCount, u64 mSize);
inline void* MutexArenaZalloc(MutexArena* s, u64 mCount, u64 mSize);
inline void* MutexArenaRealloc(MutexArena* s, void* p, u64 mCount, u64 mSize);
inline void MutexArenaFree([[maybe_unused]] MutexArena* s, [[maybe_unused]] void* p);
inline void MutexArenaFreeAll(MutexArena* s);

inline void* alloc(MutexArena* s, u64 mCount, u64 mSize) { return MutexArenaAlloc(s, mCount, mSize); }
inline void* zalloc(MutexArena* s, u64 mCount, u64 mSize) { return MutexArenaZalloc(s, mCount, mSize); }
inline void* realloc(MutexArena* s, void* p, u64 mCount, u64 mSize) { return MutexArenaRealloc(s, p, mCount, mSize); }
inline void free(MutexArena* s, void* p) { MutexArenaFree(s, p); }
inline void freeAll(MutexArena* s) { MutexArenaFreeAll(s); }

inline void*
MutexArenaAlloc(MutexArena* s, u64 mCount, u64 mSize)
{
    mtx_lock(&s->mtx);
    auto* r = ArenaAlloc(&s->arena, mCount, mSize);
    mtx_unlock(&s->mtx);

    return r;
}

inline void*
MutexArenaZalloc(MutexArena* s, u64 mCount, u64 mSize)
{
    mtx_lock(&s->mtx);
    auto* r = ArenaZalloc(&s->arena, mCount, mSize);
    mtx_unlock(&s->mtx);

    return r;
}

inline void*
MutexArenaRealloc(MutexArena* s, void* p, u64 mCount, u64 mSize)
{
    mtx_lock(&s->mtx);
    auto* r = ArenaRealloc(&s->arena, p, mCount, mSize);
    mtx_unlock(&s->mtx);

    return r;
}

inline void
MutexArenaFree([[maybe_unused]] MutexArena* s, [[maybe_unused]] void* p)
{
    /* no individual frees */
}

inline void
MutexArenaFreeAll(MutexArena* s)
{
    ArenaFreeAll(&s->arena);
    mtx_destroy(&s->mtx);
}

inline const AllocatorVTable inl_AtomicArenaAllocatorVTable {
    .alloc = decltype(AllocatorVTable::alloc)(MutexArenaAlloc),
    .zalloc = decltype(AllocatorVTable::zalloc)(MutexArenaZalloc),
    .realloc = decltype(AllocatorVTable::realloc)(MutexArenaRealloc),
    .free = decltype(AllocatorVTable::free)(MutexArenaFree),
    .freeAll = decltype(AllocatorVTable::freeAll)(MutexArenaFreeAll),
};

inline
MutexArena::MutexArena(u32 blockCap)
    : arena(blockCap)
{
    arena.super = {&inl_AtomicArenaAllocatorVTable};
    mtx_init(&mtx, mtx_plain);
}

} /* namespace adt */

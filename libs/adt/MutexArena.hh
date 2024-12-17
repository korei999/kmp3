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

    [[nodiscard]] inline void* alloc(u64 mCount, u64 mSize);
    [[nodiscard]] inline void* zalloc(u64 mCount, u64 mSize);
    [[nodiscard]] inline void* realloc(void* ptr, u64 mCount, u64 mSize);
    inline void free(void* ptr);
    inline void freeAll();
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

inline const AllocatorVTable inl_AtomicArenaAllocatorVTable {
    .alloc = decltype(AllocatorVTable::alloc)(+[](MutexArena* s, u64 mCount, u64 mSize) {
        return s->alloc(mCount, mSize);
    }),
    .zalloc = decltype(AllocatorVTable::zalloc)(+[](MutexArena* s, u64 mCount, u64 mSize) {
        return s->zalloc(mCount, mSize);
    }),
    .realloc = decltype(AllocatorVTable::realloc)(+[](MutexArena* s, void* ptr, u64 mCount, u64 mSize) {
        return s->realloc(ptr, mCount, mSize);
    }),
    .free = decltype(AllocatorVTable::free)(+[](MutexArena* s, void* ptr) {
        return s->free(ptr);
    }),
    .freeAll = decltype(AllocatorVTable::freeAll)(+[](MutexArena* s) {
        return s->freeAll();
    }),
};

inline
MutexArena::MutexArena(u32 blockCap)
    : arena(blockCap)
{
    arena.super = {&inl_AtomicArenaAllocatorVTable};
    mtx_init(&mtx, mtx_plain);
}

} /* namespace adt */

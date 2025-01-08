#pragma once

#include "Arena.hh"

#include <threads.h>

namespace adt
{

struct MutexArena : IAllocator
{
    Arena m_arena {};
    mtx_t m_mtx {};

    /* */

    MutexArena() = default;
    MutexArena(ssize blockCap, IAllocator* pBackAlloc = OsAllocatorGet())
        : m_arena(blockCap, pBackAlloc)
    {
        mtx_init(&m_mtx, mtx_plain);
    }

    /* */

    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize mCount, usize mSize) noexcept(false) override final;
    virtual void free(void* ptr) noexcept override final;
    virtual void freeAll() noexcept override final;
};

inline void*
MutexArena::malloc(usize mCount, usize mSize)
{
    mtx_lock(&m_mtx);
    auto* r = m_arena.malloc(mCount, mSize);
    mtx_unlock(&m_mtx);

    return r;
}

inline void*
MutexArena::zalloc(usize mCount, usize mSize)
{
    mtx_lock(&m_mtx);
    auto* r = m_arena.malloc(mCount, mSize);
    mtx_unlock(&m_mtx);

    return r;
}

inline void*
MutexArena::realloc(void* p, usize mCount, usize mSize)
{
    mtx_lock(&m_mtx);
    auto* r = m_arena.realloc(p, mCount, mSize);
    mtx_unlock(&m_mtx);

    return r;
}

inline void
MutexArena::free(void*) noexcept
{
    /* noop */
}

inline void
MutexArena::freeAll() noexcept
{
    m_arena.freeAll();
    mtx_destroy(&m_mtx);
}

} /* namespace adt */

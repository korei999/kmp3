#pragma once

#include "Arena.hh"
#include "Thread.hh"

namespace adt
{

struct MutexArena : IAllocator
{
    Arena m_arena {};
    Mutex m_mtx {};

    /* */

    MutexArena() = default;

    MutexArena(ssize blockCap, IAllocator* pBackAlloc = StdAllocator::inst())
        : m_arena(blockCap, pBackAlloc), m_mtx(Mutex::TYPE::PLAIN) {}

    /* */

    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false) override final;
    virtual void free(void* ptr) noexcept override final;
    virtual void freeAll() noexcept override final;
};

inline void*
MutexArena::malloc(usize mCount, usize mSize)
{
    void* r {};
    {
        LockGuard lock(&m_mtx);
        r = m_arena.malloc(mCount, mSize);
    }

    return r;
}

inline void*
MutexArena::zalloc(usize mCount, usize mSize)
{
    void* r {};
    {
        LockGuard lock(&m_mtx);
        r = m_arena.malloc(mCount, mSize);

    }

    memset(r, 0, mCount * mSize);
    return r;
}

inline void*
MutexArena::realloc(void* p, usize oldCount, usize newCount, usize mSize)
{
    void* r {};
    {
        LockGuard lock(&m_mtx);
        r = m_arena.realloc(p, oldCount, newCount, mSize);
    }

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
    m_mtx.destroy();
}

} /* namespace adt */

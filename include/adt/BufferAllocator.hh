#pragma once

#include "IArena.hh"

namespace adt
{

/* Bump allocator, using fixed size memory buffer. */
struct BufferAllocator : public IArena
{
    u8* m_pMemBuffer = nullptr;
    usize m_size = 0;
    usize m_cap = 0;
    void* m_pLastAlloc = nullptr;

    /* */

    BufferAllocator() = default;

    BufferAllocator(u8* pMemory, usize capacity) noexcept
        : IArena{INIT},
          m_pMemBuffer{pMemory},
          m_cap{capacity}
    {}

    template<typename T, usize N>
    BufferAllocator(T (&aMem)[N]) noexcept
        : IArena{INIT},
          m_pMemBuffer{reinterpret_cast<u8*>(aMem)},
          m_cap{N * sizeof(T)}
    {}

    template<typename T>
    BufferAllocator(Span<T> sp) noexcept
        : BufferAllocator{reinterpret_cast<u8*>(sp.data()), sp.size() * sizeof(T)}
    {}

    /* */

    [[nodiscard]] virtual void* malloc(usize nBytes) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize nBytes) noexcept(false) override final;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldNBytes, usize newNBytes) noexcept(false) override final;
    [[deprecated("noop")]] virtual void free(void*, usize) noexcept override final { /* noop */ }

    [[nodiscard]] virtual bool doesFree() const noexcept override final { return false; }
    [[nodiscard]] virtual bool doesRealloc() const noexcept override final { return true; }

    virtual void freeAll() noexcept override final; /* same as reset */
    [[nodiscard]] virtual IScope restoreAfterScope() noexcept override;
    virtual usize memoryUsed() const noexcept override;

    /* */

    void reset() noexcept;
    usize realCap() const noexcept;
};

struct BufferAllocatorState
{
    BufferAllocator* m_pAlloc {};
    usize m_size {};
    void* m_pLastAlloc {};

    /* */

    void restore() noexcept;
};

struct BufferAllocatorScope : IArena::IScopeDestructor
{
    BufferAllocatorState m_state {};
    SList<BufferAllocator::DeleterNode> m_lDeleters {};

    /* */

    BufferAllocatorScope(BufferAllocator* pAlloc) noexcept;
    BufferAllocatorScope(const BufferAllocatorState& state) noexcept;

    virtual ~BufferAllocatorScope() noexcept override;
};

template<>
struct IArena::Scope<BufferAllocator> : BufferAllocatorScope
{
    using BufferAllocatorScope::BufferAllocatorScope;
};

inline void
BufferAllocatorState::restore() noexcept
{
    m_pAlloc->runDeleters();

    m_pAlloc->m_size = m_size;
    m_pAlloc->m_pLastAlloc = m_pLastAlloc;
}

inline
BufferAllocatorScope::BufferAllocatorScope(BufferAllocator* pAlloc) noexcept
    : m_state {
        .m_pAlloc = pAlloc,
        .m_size = pAlloc->m_size,
        .m_pLastAlloc = pAlloc->m_pLastAlloc,
    }
{
    pAlloc->m_pLCurrentDeleters = &m_lDeleters;
}

inline
BufferAllocatorScope::BufferAllocatorScope(const BufferAllocatorState& state) noexcept
    : m_state{state}
{
    m_state.m_pAlloc->m_pLCurrentDeleters = &m_lDeleters;
}

inline
BufferAllocatorScope::~BufferAllocatorScope() noexcept
{
    m_state.restore();
}

inline void*
BufferAllocator::malloc(usize nBytes)
{
    usize realSize = alignUp8(nBytes);

    if (m_size + realSize > m_cap)
    {
        errno = ENOBUFS;
        throw AllocException("BufferAllocator::malloc(): out of memory");
    }

    void* ret = &m_pMemBuffer[m_size];
    m_size += realSize;
    m_pLastAlloc = ret;

    return ret;
}

inline void*
BufferAllocator::zalloc(usize nBytes)
{
    auto* p = malloc(nBytes);
    memset(p, 0, nBytes);
    return p;
}

inline void*
BufferAllocator::realloc(void* p, usize oldNBytes, usize newNBytes)
{
    if (!p) return malloc(newNBytes);

    ADT_ASSERT(p >= m_pMemBuffer && p < m_pMemBuffer + m_cap, "invalid pointer");

    const usize realSize = alignUp8(newNBytes);

    if (p == m_pLastAlloc)
    {
        const usize reallocatedSize = ((u8*)m_pLastAlloc - m_pMemBuffer) + realSize;
        if ((reallocatedSize) > m_cap)
        {
            errno = ENOBUFS;
            throw AllocException("BufferAllocator::realloc(): out of memory");
        }

        m_size = reallocatedSize;
        return p;
    }

    if (newNBytes <= oldNBytes) return p;

    auto* ret = malloc(newNBytes);
    memcpy(ret, p, oldNBytes);

    return ret;
}

inline void
BufferAllocator::freeAll() noexcept
{
    reset();
}

inline IArena::IScope
BufferAllocator::restoreAfterScope() noexcept
{
    BufferAllocatorState state {this};
    return alloc<BufferAllocatorScope>(state);
}

inline void
BufferAllocator::reset() noexcept
{
    runDeleters();
    m_size = 0;
    m_pLastAlloc = nullptr;
}

inline usize
BufferAllocator::realCap() const noexcept
{
    return alignDown8(m_cap);
}

inline usize
BufferAllocator::memoryUsed() const noexcept
{
    return m_size;
}

} /* namespace adt */

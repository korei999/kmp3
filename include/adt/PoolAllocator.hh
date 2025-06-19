#pragma once

#include "StdAllocator.hh"
#include "print.hh" /* IWYU pragma: keep */

#include <cstring>

namespace adt
{

struct PoolAllocator : public IArena
{
    /* fixed byte size (chunk) per alloc. Calling realloc() is an error */
    struct Node
    {
        Node* next;
        u8 pNodeMem[];
    };

    struct Block
    {
        Block* next = nullptr;
        Node* head = nullptr;
        usize used = 0;
        u8 pMem[];
    };

    /* */

    usize m_blockCap = 0; 
    usize m_chunkSize = 0;
    IAllocator* m_pBackAlloc {};
#if !defined NDEBUG && defined ADT_DBG_MEMORY
    std::source_location m_loc {};
#endif
    Block* m_pBlocks = nullptr;

    /* */

    PoolAllocator() = default;
    PoolAllocator(usize chunkSize, usize blockSize, IAllocator* pBackAlloc = StdAllocator::inst()
#if !defined NDEBUG && defined ADT_DBG_MEMORY
        , std::source_location _DBG_loc = std::source_location::current()

#endif
    ) noexcept(false)
        : m_blockCap {alignUp(blockSize, chunkSize + sizeof(Node))},
          m_chunkSize {chunkSize + sizeof(Node)},
          m_pBackAlloc(pBackAlloc),
#if !defined NDEBUG && defined ADT_DBG_MEMORY
          m_loc {_DBG_loc},
#endif
          m_pBlocks {allocBlock()}
    {}

    /* */

    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override final;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override final;
    ADT_WARN_IMPOSSIBLE_OPERATION virtual void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false) override final;
    void virtual free(void* ptr) noexcept override final;
    void virtual freeAll() noexcept override final;

    /* */

    template<typename T> ADT_WARN_IMPOSSIBLE_OPERATION constexpr T*
    reallocV(T*, isize, isize)
    {
        ADT_ASSERT_ALWAYS(false, "can't realloc"); return nullptr;
    };

private:
    [[nodiscard]] Block* allocBlock();
};

inline PoolAllocator::Block*
PoolAllocator::allocBlock()
{
    ADT_ASSERT(m_pBackAlloc, "uninitialized: m_pBackAlloc == nullptr");

    usize total = m_blockCap + sizeof(Block);
    Block* r = (Block*)m_pBackAlloc->zalloc(1, total);

#if !defined NDEBUG && defined ADT_DBG_MEMORY
    print::err("[PoolAllocator: {}, {}, {}]: new block of size: {}\n",
        print::stripSourcePath(m_loc.file_name()), m_loc.function_name(), m_loc.line(), m_blockCap
    );
#endif

    r->head = (Node*)r->pMem;

    isize chunks = m_blockCap / m_chunkSize;

    Node* head = r->head;
    Node* p = head;
    for (isize i = 0; i < chunks - 1; ++i)
    {
        p->next = (Node*)((u8*)p + m_chunkSize);
        p = p->next;
    }
    p->next = nullptr;

    return r;
}

inline void*
PoolAllocator::malloc(usize, usize)
{
    Block* pBlock = m_pBlocks;
    Block* pPrev = nullptr;
    while (pBlock)
    {
        if (m_blockCap - pBlock->used >= m_chunkSize) break;
        else
        {
            pPrev = pBlock;
            pBlock = pBlock->next;
        }
    }

    if (!pBlock)
    {
        pPrev->next = allocBlock();
        pBlock = pPrev->next;
    }

    Node* head = pBlock->head;
    pBlock->head = head->next;
    pBlock->used += m_chunkSize;

    return head->pNodeMem;
}

inline void*
PoolAllocator::zalloc(usize, usize)
{
    void* p = malloc(0, 0);
    memset(p, 0, m_chunkSize - sizeof(Node));
    return p;
}

inline void*
PoolAllocator::realloc(void*, usize, usize, usize)
{
    ADT_ASSERT_ALWAYS(false, "realloc() is not supported");
    throw AllocException("PoolAllocator: realloc() is not supported");

    return nullptr;
}

inline void
PoolAllocator::free(void* p) noexcept
{
    if (!p) return;

    auto* node = (Node*)((u8*)p - sizeof(Node));

    auto* pBlock = m_pBlocks;
    while (pBlock)
    {
        if ((u8*)p > (u8*)pBlock->pMem && ((u8*)pBlock + m_blockCap) > (u8*)p)
            break;

        pBlock = pBlock->next;
    }

    ADT_ASSERT(pBlock, "bad pointer?");
    
    node->next = pBlock->head;
    pBlock->head = node;
    pBlock->used -= m_chunkSize;
}

inline void
PoolAllocator::freeAll() noexcept
{
    Block* p = m_pBlocks, * next = nullptr;
    while (p)
    {
        next = p->next;
        m_pBackAlloc->free(p);
        p = next;
    }
    m_pBlocks = nullptr;
}

} /* namespace adt */

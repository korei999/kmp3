#pragma once

#include "RBTree.hh"
#include "StdAllocator.hh"

#if defined ADT_DBG_MEMORY
    #include "logs.hh"
#endif

namespace adt
{

/* Best-fit logarithmic time allocator, all IAllocator methods are supported.
 * Can be slow and memory wasteful for small allocations (48 bytes of metadata per allocation).
 * Preallocating big blocks would help. */
struct FreeListBlock
{
    FreeListBlock* pNext {};
    usize size {}; /* including sizeof(FreeListBlock) */
    usize nBytesOccupied {};
    u8 pMem[];
};

struct FreeListData
{
    static constexpr usize IS_FREE_MASK = 1ULL << 63;

    /* */

    FreeListData* m_pPrev {};
    FreeListData* m_pNext {}; /* TODO: calculate from the size (save 8 bytes) */
    usize m_sizeAndIsFree {}; /* isFree bool as leftmost (most significant) bit */
    u8 m_pMem[];
    
    /* */

    constexpr usize size() const noexcept { return m_sizeAndIsFree & ~IS_FREE_MASK; }
    constexpr bool isFree() const noexcept { return m_sizeAndIsFree & IS_FREE_MASK; }
    constexpr void setFree(bool _bFree) noexcept { _bFree ? m_sizeAndIsFree |= IS_FREE_MASK : m_sizeAndIsFree &= ~IS_FREE_MASK; };
    constexpr void setSizeSetFree(usize _size, bool _bFree) noexcept { m_sizeAndIsFree = _size; setFree(_bFree); }
    constexpr void setSize(usize _size) noexcept { setSizeSetFree(_size, isFree()); }
    constexpr void addSize(usize _size) noexcept { setSize(_size + size()); }
};

struct FreeList : public IAllocator
{
    using Node = RBNode<FreeListData>; /* node is the header + the memory chunk */

    /* */

    usize m_blockSize {};
    IAllocator* m_pBackAlloc {};
    usize m_totalAllocated {};
    RBTree<FreeListData> m_tree {}; /* free nodes sorted by size */
    FreeListBlock* m_pBlocks {};

    /* */

    FreeList() = default;

    FreeList(usize _blockSize, IAllocator* pBackAlloc = StdAllocator::inst()) noexcept(false)
        : m_blockSize(alignUp8(_blockSize + sizeof(FreeListBlock) + sizeof(FreeList::Node))),
          m_pBackAlloc(pBackAlloc),
          m_pBlocks(allocBlock(m_blockSize)) {}

    /* */

    [[nodiscard]] virtual void* malloc(usize mCount, usize mSize) noexcept(false) override;
    [[nodiscard]] virtual void* zalloc(usize mCount, usize mSize) noexcept(false) override;
    [[nodiscard]] virtual void* realloc(void* ptr, usize oldCount, usize newCount, usize mSize) noexcept(false) override;
    virtual void free(void* ptr) noexcept override;
    virtual void freeAll() noexcept override;
    usize nBytesAllocated();

#ifndef NDEBUG
    void verify();
#endif

    /* */

protected:
    [[nodiscard]] FreeListBlock* allocBlock(usize size);
    [[nodiscard]] FreeListBlock* blockPrepend(usize size);
    [[nodiscard]] FreeListBlock* blockFromNode(FreeList::Node* pNode);
    [[nodiscard]] FreeList::Node* findFittingNode(const usize size);
    FreeList::Node* splitNode(FreeList::Node* pNode, usize realSize);
};

template<>
inline constexpr isize
utils::compare(const FreeListData& l, const FreeListData& r)
{
    return (isize)l.size() - (isize)r.size();
}

inline FreeList::Node*
_FreeListNodeFromBlock(FreeListBlock* pBlock)
{
    return (FreeList::Node*)pBlock->pMem;
}

inline usize
FreeList::nBytesAllocated()
{
    return m_totalAllocated;
}

inline FreeListBlock*
FreeList::blockFromNode(FreeList::Node* pNode)
{
    auto* pBlock = m_pBlocks;
    while (pBlock)
    {
        if ((u8*)pNode > (u8*)pBlock && (u8*)pNode < (u8*)pBlock + pBlock->size)
            return pBlock;
        pBlock = pBlock->pNext;
    }

    return nullptr;
}

inline FreeListBlock*
FreeList::allocBlock(usize size)
{
    ADT_ASSERT(m_pBackAlloc, "uninitialized: m_pBackAlloc == nullptr");

    FreeListBlock* pBlock = (FreeListBlock*)m_pBackAlloc->zalloc(1, size);
    pBlock->size = size;

    FreeList::Node* pNode = _FreeListNodeFromBlock(pBlock);
    pNode->m_data.setSizeSetFree(pBlock->size - sizeof(FreeListBlock) - sizeof(FreeList::Node), true);
    pNode->m_data.m_pNext = pNode->m_data.m_pPrev = nullptr;

    m_tree.insert(true, pNode);

#if defined ADT_DBG_MEMORY
        CERR("[FreeList]: new block of '{}' bytes\n", size);
#endif

    return pBlock;
}

inline FreeListBlock*
FreeList::blockPrepend(usize size)
{
    auto* pNewBlock = allocBlock(size);

    pNewBlock->pNext = m_pBlocks;
    m_pBlocks = pNewBlock;

    return pNewBlock;
}

inline void
FreeList::freeAll() noexcept
{
    auto* it = m_pBlocks;
    while (it)
    {
        auto* next = it->pNext;
        m_pBackAlloc->free(it);
        it = next;
    }
    m_pBlocks = nullptr;
    m_tree = {};
}

inline FreeListData*
_FreeListDataFromPtr(void* p)
{
    return (FreeListData*)((u8*)p - sizeof(FreeListData));
}

inline FreeList::Node*
_FreeListNodeFromPtr(void* p)
{
    return (FreeList::Node*)((u8*)p - sizeof(FreeList::Node));
}

inline FreeList::Node*
FreeList::findFittingNode(const usize size)
{
    auto* it = m_tree.getRoot();
    const isize realSize = size + sizeof(FreeList::Node);

    FreeList::Node* pLastFitting {};
    while (it)
    {
        ADT_ASSERT(it->m_data.isFree(), "non free node in the free list");

        isize nodeSize = it->m_data.size();

        if (nodeSize >= realSize)
            pLastFitting = it;

        /* save size for the header */
        isize cmp = realSize - nodeSize;

        if (cmp == 0) break;
        else if (cmp < 0) it = it->left();
        else it = it->right();
    }

    return pLastFitting;
}

#ifndef NDEBUG
inline void
FreeList::verify()
{
    for (auto* pBlock = m_pBlocks; pBlock; pBlock = pBlock->pNext)
    {
        auto* pListNode = &_FreeListNodeFromBlock(pBlock)->m_data;
        auto* pPrev = pListNode;

        while (pListNode)
        {
            if (pListNode->m_pNext)
            {
                bool bNextAdjecent = ((u8*)pListNode + pListNode->size()) == ((u8*)pListNode->m_pNext);
                if (!bNextAdjecent)
                    LOG_FATAL("size and next don't match: [next: {}, calc: {}], size: {}, calcSize: {}\n",
                        pListNode->m_pNext, ((u8*)pListNode + pListNode->size()), pListNode->size(), (u8*)pListNode->m_pNext - (u8*)pListNode
                    );
            }

            pPrev = pListNode;
            pListNode = pListNode->m_pNext;
        }
        pListNode = pPrev;
        while (pListNode)
        {
            if (pListNode->m_pPrev)
            {
                bool bPrevAdjecent = ((u8*)pListNode->m_pPrev + pListNode->m_pPrev->size()) == ((u8*)pListNode);
                ADT_ASSERT(bPrevAdjecent, " ");
            }

            pPrev = pListNode;
            pListNode = pListNode->m_pPrev;
        }
    }
}
#endif

/* Split node in two pieces.
 * Return left part with realSize.
 * Insert right part with the rest of the space to the tree */
inline FreeList::Node*
FreeList::splitNode(FreeList::Node* pNode, usize realSize)
{
    isize splitSize = isize(pNode->m_data.size()) - isize(realSize);

    ADT_ASSERT(splitSize >= 0, " ");

    ADT_ASSERT(pNode->m_data.isFree(), "splitting non free node (corruption)");
    m_tree.remove(pNode);

    if (splitSize <= (isize)sizeof(FreeList::Node))
    {
        pNode->m_data.setFree(false);
        return pNode;
    }

    FreeList::Node* pSplit = (FreeList::Node*)((u8*)pNode + realSize);
    pSplit->m_data.setSizeSetFree(splitSize, true);

    pSplit->m_data.m_pNext = pNode->m_data.m_pNext;
    pSplit->m_data.m_pPrev = &pNode->m_data;

    if (pNode->m_data.m_pNext) pNode->m_data.m_pNext->m_pPrev = &pSplit->m_data;
    pNode->m_data.m_pNext = &pSplit->m_data;
    pNode->m_data.setSizeSetFree(realSize, false);

    m_tree.insert(true, pSplit);
    return pSplit;
}

inline void*
FreeList::malloc(usize nMembers, usize mSize)
{
    usize requested = alignUp8(nMembers * mSize);
    if (requested == 0)
        return nullptr;

    usize realSize = requested + sizeof(FreeList::Node);

    /* find block that fits */
    auto* pBlock = m_pBlocks;
    while (pBlock)
    {
        bool bFits = (((pdiff)pBlock->size - (pdiff)sizeof(FreeListBlock)) -
            (pdiff)pBlock->nBytesOccupied) >= (pdiff)realSize;

        if (!bFits) pBlock = pBlock->pNext;
        else break;
    }

    if (!pBlock)
    {
#if defined ADT_DBG_MEMORY
        CERR("[FreeList]: no fitting block for '{}' bytes\n", realSize);
#endif

GOTO_again:
        pBlock = blockPrepend(utils::max(m_blockSize, requested*2 + sizeof(FreeListBlock) + sizeof(FreeList::Node)));
    }

    auto* pFree = findFittingNode(requested);
    if (!pFree) goto GOTO_again;

    splitNode(pFree, realSize);

    pBlock->nBytesOccupied += pFree->data().size();
    m_totalAllocated += pFree->data().size();

    return pFree->m_data.m_pMem;
}

inline void*
FreeList::zalloc(usize nMembers, usize mSize)
{
    auto* p = malloc(nMembers, mSize);
    memset(p, 0, nMembers * mSize);
    return p;
}

inline void
FreeList::free(void* ptr) noexcept
{
    if (ptr == nullptr) return;

    auto* pNode = _FreeListNodeFromPtr(ptr);
    ADT_ASSERT(!pNode->m_data.isFree(), "double free");

    auto* pBlock = blockFromNode(pNode);

    pNode->m_data.setFree(true);
    pBlock->nBytesOccupied -= pNode->m_data.size();
    m_totalAllocated -= pNode->m_data.size();

    /* next adjecent node coalescence */
    if (pNode->m_data.m_pNext && pNode->m_data.m_pNext->isFree())
    {
        m_tree.remove(_FreeListNodeFromPtr(pNode->m_data.m_pNext->m_pMem));

        pNode->m_data.addSize(pNode->m_data.m_pNext->size());
        if (pNode->m_data.m_pNext->m_pNext)
            pNode->m_data.m_pNext->m_pNext->m_pPrev = &pNode->m_data;
        pNode->m_data.m_pNext = pNode->m_data.m_pNext->m_pNext;
    }

    /* prev adjecent node coalescence */
    if (pNode->m_data.m_pPrev && pNode->m_data.m_pPrev->isFree())
    {
        auto* pPrev = _FreeListNodeFromPtr(pNode->m_data.m_pPrev->m_pMem);
        m_tree.remove(pPrev);

        pNode = pPrev;

        pNode->m_data.addSize(pNode->m_data.m_pNext->size());
        if (pNode->m_data.m_pNext->m_pNext)
            pNode->m_data.m_pNext->m_pNext->m_pPrev = &pNode->m_data;
        pNode->m_data.m_pNext = pNode->m_data.m_pNext->m_pNext;
    }

    m_tree.insert(true, pNode);
}

inline void*
FreeList::realloc(void* ptr, usize oldCount, usize newCount, usize mSize)
{
    if (!ptr)
        return malloc(newCount, mSize);

    const usize requested = alignUp8(newCount * mSize);

    if (requested == 0)
    {
        free(ptr);
        return nullptr;
    }

    auto* pNode = _FreeListNodeFromPtr(ptr);
    isize nodeSize = (isize)pNode->m_data.size() - (isize)sizeof(FreeList::Node);
    ADT_ASSERT(nodeSize > 0, "0 or negative size allocation (corruption), nodeSize: {}", nodeSize);

    if ((isize)newCount*(isize)mSize <= nodeSize)
        return ptr;

    ADT_ASSERT(!pNode->m_data.isFree(), "trying to realloc non free node");

    /* try to bump if next is free and can fit */
    {
        usize realSize = requested + sizeof(FreeList::Node);
        auto* pNext = pNode->m_data.m_pNext;

        if (pNext && pNext->isFree() && pNode->m_data.size() + pNext->size() >= realSize)
        {
            auto* pBlock = blockFromNode(pNode);
            ADT_ASSERT(pBlock, "failed to find the block");

            pBlock->nBytesOccupied += realSize - pNode->m_data.size();
            m_totalAllocated += realSize - pNode->m_data.size();

            /* remove next from the free list */
            pNext->setFree(false);
            m_tree.remove(_FreeListNodeFromPtr(pNext->m_pMem));

            /* merge with next */
            pNode->m_data.addSize(pNext->size());
            pNode->m_data.m_pNext = pNext->m_pNext;
            if (pNext->m_pNext) pNext->m_pNext->m_pPrev = &pNode->m_data;
            pNode->m_data.setFree(true);

            m_tree.insert(true, _FreeListNodeFromPtr(ptr));

            splitNode(pNode, realSize);

            ADT_ASSERT(ptr == pNode->m_data.m_pMem, " ");
            return ptr;
        }
    }

    auto* pRet = malloc(newCount, mSize);
    memcpy(pRet, ptr, utils::min(oldCount, newCount) * mSize);
    free(ptr);

    return pRet;
}

} /* namespace adt */

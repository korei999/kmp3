#pragma once

#include "RBTree.hh"
#include "OsAllocator.hh"

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
    u64 size {}; /* including sizeof(FreeListBlock) */
    u64 nBytesOccupied {};
    u8 pMem[];
};

constexpr u64 FREE_LIST_IS_FREE_MASK = 1ULL << 63;

struct FreeListData
{
    static constexpr u64 IS_FREE_MASK = 1ULL << 63;

    FreeListData* pPrev {};
    FreeListData* pNext {}; /* TODO: calculate from the size (save 8 bytes) */
    u64 sizeAndIsFree {}; /* isFree bool as leftmost (most significant) bit */
    u8 pMem[];

    constexpr u64 getSize() const { return sizeAndIsFree & ~IS_FREE_MASK; }
    constexpr bool isFree() const { return sizeAndIsFree & IS_FREE_MASK; }
    constexpr void setFree(bool _bFree) { _bFree ? sizeAndIsFree |= IS_FREE_MASK : sizeAndIsFree &= ~IS_FREE_MASK; };
    constexpr void setSizeSetFree(u64 _size, bool _bFree) { sizeAndIsFree = _size; setFree(_bFree); }
    constexpr void setSize(u64 _size) { setSizeSetFree(_size, isFree()); }
    constexpr void addSize(u64 _size) { setSize(_size + getSize()); }
};

class FreeList : public IAllocator
{
public:
    using Node = RBNode<FreeListData>; /* node is the header + memory chunk of the allocation */

    /* */

private:
    u64 m_blockSize {};
    IAllocator* m_pBackAlloc {};
    u64 m_totalAllocated {};
    RBTreeBase<FreeListData> m_tree {}; /* free nodes sorted by size */
    FreeListBlock* m_pBlocks {};

    /* */

public:
    FreeList() = default;
    FreeList(u64 _blockSize, IAllocator* pBackAlloc = OsAllocatorGet())
        : m_blockSize(align8(_blockSize + sizeof(FreeListBlock) + sizeof(FreeList::Node))),
          m_pBackAlloc(pBackAlloc),
          m_pBlocks(allocBlock(m_blockSize)) {}

    /* */

    [[nodiscard]] virtual void* malloc(u64 mCount, u64 mSize) override;
    [[nodiscard]] virtual void* zalloc(u64 mCount, u64 mSize) override;
    [[nodiscard]] virtual void* realloc(void* ptr, u64 mCount, u64 mSize) override;
    virtual void free(void* ptr) override;
    virtual void freeAll() override;
    u64 nBytesAllocated();

#ifndef NDEBUG
    void verify();
#endif

    /* */

private:
    [[nodiscard]] FreeListBlock* allocBlock(u64 size);
    [[nodiscard]] FreeListBlock* blockPrepend(u64 size);
    [[nodiscard]] FreeListBlock* blockFromNode(FreeList::Node* pNode);
    [[nodiscard]] FreeList::Node* findFittingNode(const u64 size);
    FreeList::Node* splitNode(FreeList::Node* pNode, u64 realSize);
};

template<>
constexpr s64
utils::compare(const FreeListData& l, const FreeListData& r)
{
    return (s64)l.getSize() - (s64)r.getSize();
}

inline FreeList::Node*
_FreeListNodeFromBlock(FreeListBlock* pBlock)
{
    return (FreeList::Node*)pBlock->pMem;
}

inline u64
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
FreeList::allocBlock(u64 size)
{
    FreeListBlock* pBlock = (FreeListBlock*)m_pBackAlloc->zalloc(1, size);
    pBlock->size = size;

    FreeList::Node* pNode = _FreeListNodeFromBlock(pBlock);
    pNode->m_data.setSizeSetFree(pBlock->size - sizeof(FreeListBlock) - sizeof(FreeList::Node), true);
    pNode->m_data.pNext = pNode->m_data.pPrev = nullptr;

    m_tree.insert(true, pNode);

#if defined ADT_DBG_MEMORY
        CERR("[FreeList]: new block of '{}' bytes\n", size);
#endif

    return pBlock;
}

inline FreeListBlock*
FreeList::blockPrepend(u64 size)
{
    auto* pNewBlock = allocBlock(size);

    pNewBlock->pNext = m_pBlocks;
    m_pBlocks = pNewBlock;

    return pNewBlock;
}

inline void
FreeList::freeAll()
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
FreeList::findFittingNode(const u64 size)
{
    auto* it = m_tree.m_pRoot;
    const s64 realSize = size + sizeof(FreeList::Node);

    FreeList::Node* pLastFitting {};
    while (it)
    {
        assert(it->m_data.isFree() && "[FreeList]: non free node in the free list");

        s64 nodeSize = it->m_data.getSize();

        if (nodeSize >= realSize)
            pLastFitting = it;

        /* save size for the header */
        s64 cmp = realSize - nodeSize;

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
            if (pListNode->pNext)
            {
                bool bNextAdjecent = ((u8*)pListNode + pListNode->getSize()) == ((u8*)pListNode->pNext);
                if (!bNextAdjecent)
                    LOG_FATAL("size and next don't match: [next: {}, calc: {}], size: {}, calcSize: {}\n",
                        pListNode->pNext, ((u8*)pListNode + pListNode->getSize()), pListNode->getSize(), (u8*)pListNode->pNext - (u8*)pListNode
                    );
            }

            pPrev = pListNode;
            pListNode = pListNode->pNext;
        }
        pListNode = pPrev;
        while (pListNode)
        {
            if (pListNode->pPrev)
            {
                bool bPrevAdjecent = ((u8*)pListNode->pPrev + pListNode->pPrev->getSize()) == ((u8*)pListNode);
                assert(bPrevAdjecent);
            }

            pPrev = pListNode;
            pListNode = pListNode->pPrev;
        }
    }
}
#endif

/* Split node in two pieces.
 * Return left part with realSize.
 * Insert right part with the rest of the space to the tree */
inline FreeList::Node*
FreeList::splitNode(FreeList::Node* pNode, u64 realSize)
{
    s64 splitSize = s64(pNode->m_data.getSize()) - s64(realSize);

    assert(splitSize >= 0);

    assert(pNode->m_data.isFree() && "splitting non free node (corruption)");
    m_tree.remove(pNode);

    if (splitSize <= (s64)sizeof(FreeList::Node))
    {
        pNode->m_data.setFree(false);
        return pNode;
    }

    FreeList::Node* pSplit = (FreeList::Node*)((u8*)pNode + realSize);
    pSplit->m_data.setSizeSetFree(splitSize, true);

    pSplit->m_data.pNext = pNode->m_data.pNext;
    pSplit->m_data.pPrev = &pNode->m_data;

    if (pNode->m_data.pNext) pNode->m_data.pNext->pPrev = &pSplit->m_data;
    pNode->m_data.pNext = &pSplit->m_data;
    pNode->m_data.setSizeSetFree(realSize, false);

    m_tree.insert(true, pSplit);
    return pSplit;
}

inline void*
FreeList::malloc(u64 nMembers, u64 mSize)
{
    u64 requested = align8(nMembers * mSize);
    if (requested == 0) return nullptr;
    u64 realSize = requested + sizeof(FreeList::Node);

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

again:
        pBlock = blockPrepend(utils::max(m_blockSize, requested*2 + sizeof(FreeListBlock) + sizeof(FreeList::Node)));
    }

    auto* pFree = findFittingNode(requested);
    if (!pFree) goto again;

    splitNode(pFree, realSize);

    pBlock->nBytesOccupied += pFree->data().getSize();
    m_totalAllocated += pFree->data().getSize();

    return pFree->m_data.pMem;
}

inline void*
FreeList::zalloc(u64 nMembers, u64 mSize)
{
    auto* p = malloc(nMembers, mSize);
    memset(p, 0, nMembers * mSize);
    return p;
}

inline void
FreeList::free(void* ptr)
{
    if (ptr == nullptr) return;

    auto* pNode = _FreeListNodeFromPtr(ptr);
    assert(!pNode->m_data.isFree() && "[FreeList]: double free");

    auto* pBlock = blockFromNode(pNode);

    pNode->m_data.setFree(true);
    pBlock->nBytesOccupied -= pNode->m_data.getSize();
    m_totalAllocated -= pNode->m_data.getSize();

    /* next adjecent node coalescence */
    if (pNode->m_data.pNext && pNode->m_data.pNext->isFree())
    {
        m_tree.remove(_FreeListNodeFromPtr(pNode->m_data.pNext->pMem));

        pNode->m_data.addSize(pNode->m_data.pNext->getSize());
        if (pNode->m_data.pNext->pNext)
            pNode->m_data.pNext->pNext->pPrev = &pNode->m_data;
        pNode->m_data.pNext = pNode->m_data.pNext->pNext;
    }

    /* prev adjecent node coalescence */
    if (pNode->m_data.pPrev && pNode->m_data.pPrev->isFree())
    {
        auto* pPrev = _FreeListNodeFromPtr(pNode->m_data.pPrev->pMem);
        m_tree.remove(pPrev);

        pNode = pPrev;

        pNode->m_data.addSize(pNode->m_data.pNext->getSize());
        if (pNode->m_data.pNext->pNext)
            pNode->m_data.pNext->pNext->pPrev = &pNode->m_data;
        pNode->m_data.pNext = pNode->m_data.pNext->pNext;
    }

    m_tree.insert(true, pNode);
}

inline void*
FreeList::realloc(void* ptr, u64 nMembers, u64 mSize)
{
    if (!ptr) return malloc(nMembers, mSize);

    auto* pNode = _FreeListNodeFromPtr(ptr);
    s64 nodeSize = (s64)pNode->m_data.getSize() - (s64)sizeof(FreeList::Node);
    assert(nodeSize > 0 && "[FreeList]: 0 or negative size allocation (corruption)");

    if ((s64)nMembers*(s64)mSize <= nodeSize) return ptr;

    assert(!pNode->m_data.isFree() && "[FreeList]: trying to realloc non free node");

    /* try to bump if next is free and can fit */
    {
        u64 requested = align8(nMembers * mSize);
        u64 realSize = requested + sizeof(FreeList::Node);
        auto* pNext = pNode->m_data.pNext;

        if (pNext && pNext->isFree() && pNode->m_data.getSize() + pNext->getSize() >= realSize)
        {
            auto* pBlock = blockFromNode(pNode);
            assert(pBlock && "[FreeList]: failed to find the block");

            pBlock->nBytesOccupied += realSize - pNode->m_data.getSize();
            m_totalAllocated += realSize - pNode->m_data.getSize();

            /* remove next from the free list */
            pNext->setFree(false);
            m_tree.remove(_FreeListNodeFromPtr(pNext->pMem));

            /* merge with next */
            pNode->m_data.addSize(pNext->getSize());
            pNode->m_data.pNext = pNext->pNext;
            if (pNext->pNext) pNext->pNext->pPrev = &pNode->m_data;
            pNode->m_data.setFree(true);

            m_tree.insert(true, _FreeListNodeFromPtr(ptr));

            splitNode(pNode, realSize);

            assert(ptr == pNode->m_data.pMem);
            return ptr;
        }
    }

    auto* pRet = malloc(nMembers, mSize);
    memcpy(pRet, ptr, nodeSize);
    free(ptr);

    return pRet;
}

} /* namespace adt */

#pragma once

#include "RBTree.hh"

#if defined ADT_DBG_MEMORY
    #include "logs.hh"
#endif

namespace adt
{

/* Best-fit logarithmic time allocator, all IAllocator methods are supported.
 * Can be slow and memory wasteful for small allocations (48 bytes of metadata per allocation).
 * Preallocating big blocks would help. */
struct FreeList;

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

struct FreeList
{
    using Node = RBNode<FreeListData>; /* node is the header + memory chunk of the allocation */

    IAllocator super {};
    u64 blockSize {};
    u64 totalAllocated {};
    RBTreeBase<FreeListData> tree {}; /* free nodes sorted by size */
    FreeListBlock* pBlocks {};

    FreeList() = default;
    FreeList(u64 _blockSize);

    [[nodiscard]] inline void* alloc(u64 mCount, u64 mSize);
    [[nodiscard]] inline void* zalloc(u64 mCount, u64 mSize);
    [[nodiscard]] inline void* realloc(void* ptr, u64 mCount, u64 mSize);
    inline void free(void* ptr);
    inline void freeAll();
};


#if defined ADT_DBG_MEMORY
inline void
_FreeListPrintTree(FreeList* s, IAllocator* pAlloc)
{
    /*RBPrintNodes(pAlloc, s->tree.pRoot, stderr, {}, false);*/
}
#else
    #define _FreeListPrintTree(...) (void)0
#endif

template<>
constexpr s64
utils::compare(const FreeListData& l, const FreeListData& r)
{
    return l.getSize() - r.getSize();
}

inline FreeList::Node*
_FreeListNodeFromBlock(FreeListBlock* pBlock)
{
    return (FreeList::Node*)pBlock->pMem;
}

inline u64
_FreeListGetBytesAllocated(FreeList* s)
{
    return s->totalAllocated;
}

inline FreeListBlock*
_FreeListBlockFromNode(FreeList* s, FreeList::Node* pNode)
{
    auto* pBlock = s->pBlocks;
    while (pBlock)
    {
        if ((u8*)pNode > (u8*)pBlock && (u8*)pNode < (u8*)pBlock + pBlock->size)
            return pBlock;
        pBlock = pBlock->pNext;
    }

    return nullptr;
}

inline FreeListBlock*
_FreeListAllocBlock(FreeList* s, u64 size)
{
    FreeListBlock* pBlock = (FreeListBlock*)::calloc(1, size);
    pBlock->size = size;

    FreeList::Node* pNode = _FreeListNodeFromBlock(pBlock);
    pNode->data.setSizeSetFree(pBlock->size - sizeof(FreeListBlock) - sizeof(FreeList::Node), true);
    pNode->data.pNext = pNode->data.pPrev = nullptr;

    s->tree.insert(pNode, true);

#if defined ADT_DBG_MEMORY
        CERR("[FreeList]: new block of '{}' bytes\n", size);
#endif

    return pBlock;
}

inline FreeListBlock*
_FreeListBlockPrepend(FreeList* s, u64 size)
{
    auto* pNewBlock = _FreeListAllocBlock(s, size);

    pNewBlock->pNext = s->pBlocks;
    s->pBlocks = pNewBlock;

    return pNewBlock;
}

inline void
FreeList::freeAll()
{
    auto* s = this;

    auto* it = s->pBlocks;
    while (it)
    {
        auto* next = it->pNext;
        ::free(it);
        it = next;
    }
    s->pBlocks = nullptr;
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
_FreeListFindFittingNode(FreeList* s, const u64 size)
{
    auto* it = s->tree.pRoot;
    const s64 realSize = size + sizeof(FreeList::Node);

    FreeList::Node* pLastFitting {};
    while (it)
    {
        assert(it->data.isFree() && "[FreeList]: non free node in the free list");

        s64 nodeSize = it->data.getSize();

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
_FreeListVerify(FreeList* s)
{
    for (auto* pBlock = s->pBlocks; pBlock; pBlock = pBlock->pNext)
    {
        auto* pListNode = &_FreeListNodeFromBlock(pBlock)->data;
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
#else
#define _FreeListVerify(...) (void)0
#endif

/* Split node in two pieces.
 * Return left part with realSize.
 * Insert right part with the rest of the space to the tree */
inline FreeList::Node*
_FreeListSplitNode(FreeList* s, FreeList::Node* pNode, u64 realSize)
{
    s64 splitSize = s64(pNode->data.getSize()) - s64(realSize);

    assert(splitSize >= 0);

    assert(pNode->data.isFree() && "splitting non free node (corruption)");
    s->tree.remove(pNode);

    if (splitSize <= (s64)sizeof(FreeList::Node))
    {
        pNode->data.setFree(false);
        return pNode;
    }

    FreeList::Node* pSplit = (FreeList::Node*)((u8*)pNode + realSize);
    pSplit->data.setSizeSetFree(splitSize, true);

    pSplit->data.pNext = pNode->data.pNext;
    pSplit->data.pPrev = &pNode->data;

    if (pNode->data.pNext) pNode->data.pNext->pPrev = &pSplit->data;
    pNode->data.pNext = &pSplit->data;
    pNode->data.setSizeSetFree(realSize, false);

    s->tree.insert(pSplit, true);
    return pSplit;
}

inline void*
FreeList::alloc(u64 nMembers, u64 mSize)
{
    auto* s = this;

    u64 requested = align8(nMembers * mSize);
    if (requested == 0) return nullptr;
    u64 realSize = requested + sizeof(FreeList::Node);

    /* find block that fits */
    auto* pBlock = s->pBlocks;
    while (pBlock)
    {
        bool bFits = (((pdiff)pBlock->size - (pdiff)sizeof(FreeListBlock)) - (pdiff)pBlock->nBytesOccupied) >= (pdiff)realSize;

        if (!bFits) pBlock = pBlock->pNext;
        else break;
    }

    if (!pBlock)
    {
#if defined ADT_DBG_MEMORY
        CERR("[FreeList]: no fitting block for '{}' bytes\n", realSize);
#endif

again:
        pBlock = _FreeListBlockPrepend(s, utils::max(s->blockSize, requested*2 + sizeof(FreeListBlock) + sizeof(FreeList::Node)));
    }

    auto* pFree = _FreeListFindFittingNode(s, requested);
    if (!pFree) goto again;

    pBlock->nBytesOccupied += realSize;
    s->totalAllocated += realSize;

    _FreeListSplitNode(s, pFree, realSize);

    return pFree->data.pMem;
}

inline void*
FreeList::zalloc(u64 nMembers, u64 mSize)
{
    auto* s = this;

    auto* p = s->alloc(nMembers, mSize);
    memset(p, 0, nMembers * mSize);
    return p;
}

inline void
FreeList::free(void* ptr)
{
    auto* s = this;

    if (ptr == nullptr) return;

    auto* pThis = _FreeListNodeFromPtr(ptr);
    assert(!pThis->data.isFree());

    auto* pBlock = _FreeListBlockFromNode(s, pThis);

    pThis->data.setFree(true);
    pBlock->nBytesOccupied -= pThis->data.getSize();
    s->totalAllocated -= pThis->data.getSize();

    /* next adjecent node coalescence */
    if (pThis->data.pNext && pThis->data.pNext->isFree())
    {
        this->tree.remove(_FreeListNodeFromPtr(pThis->data.pNext->pMem));

        pThis->data.addSize(pThis->data.pNext->getSize());
        if (pThis->data.pNext->pNext)
            pThis->data.pNext->pNext->pPrev = &pThis->data;
        pThis->data.pNext = pThis->data.pNext->pNext;
    }

    /* prev adjecent node coalescence */
    if (pThis->data.pPrev && pThis->data.pPrev->isFree())
    {
        auto* pPrev = _FreeListNodeFromPtr(pThis->data.pPrev->pMem);
        this->tree.remove(pPrev);

        pThis = pPrev;

        pThis->data.addSize(pThis->data.pNext->getSize());
        if (pThis->data.pNext->pNext)
            pThis->data.pNext->pNext->pPrev = &pThis->data;
        pThis->data.pNext = pThis->data.pNext->pNext;
    }

    this->tree.insert(pThis, true);
}

inline void*
FreeList::realloc(void* ptr, u64 nMembers, u64 mSize)
{
    auto* s = this;

    if (!ptr) return s->alloc(nMembers, mSize);

    auto* pThis = _FreeListNodeFromPtr(ptr);
    s64 nodeSize = (s64)pThis->data.getSize() - (s64)sizeof(FreeList::Node);
    assert(nodeSize > 0 && "[FreeList]: 0 or negative size allocation (corruption)");

    if ((s64)nMembers*(s64)mSize <= nodeSize) return ptr;

    assert(!pThis->data.isFree() && "[FreeList]: trying to realloc non free node");

    /* try to bump if next is free and can fit */
    {
        u64 requested = align8(nMembers * mSize);
        u64 realSize = requested + sizeof(FreeList::Node);
        auto* pNext = pThis->data.pNext;

        if (pNext && pNext->isFree() && pThis->data.getSize() + pNext->getSize() >= realSize)
        {
            auto* pBlock = _FreeListBlockFromNode(s, pThis);
            assert(pBlock && "[FreeList]: failed to find the block");

            pBlock->nBytesOccupied += realSize - pThis->data.getSize();
            s->totalAllocated += realSize - pThis->data.getSize();

            /* remove next from the free list */
            pNext->setFree(false);
            this->tree.remove(_FreeListNodeFromPtr(pNext->pMem));

            /* merge with next */
            pThis->data.addSize(pNext->getSize());
            pThis->data.pNext = pNext->pNext;
            if (pNext->pNext) pNext->pNext->pPrev = &pThis->data;
            pThis->data.setFree(true);

            this->tree.insert(_FreeListNodeFromPtr(ptr), true);

            _FreeListSplitNode(s, pThis, realSize);

            assert(ptr == pThis->data.pMem);
            return ptr;
        }
    }

    auto* pRet = s->alloc(nMembers, mSize);
    memcpy(pRet, ptr, nodeSize);
    s->free(ptr);

    return pRet;
}

inline const AllocatorVTable inl_FreeListVTable = AllocatorVTableGenerate<FreeList>();

inline FreeList::FreeList(u64 _blockSize)
    : super(&inl_FreeListVTable),
      blockSize(align8(_blockSize + sizeof(FreeListBlock) + sizeof(FreeList::Node))),
      pBlocks(_FreeListAllocBlock(this, this->blockSize)) {}

} /* namespace adt */

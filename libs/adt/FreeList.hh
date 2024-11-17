#pragma once

#include "RBTree.hh"

#include "logs.hh"

namespace adt
{

struct FreeListBlock
{
    FreeListBlock* pNext {};
    u64 size {}; /* including sizeof(FreeListBlock) */
    u64 nBytesOccupied {};
    u8 pMem[];
};

constexpr u64 IS_FREE_BITMASK = 1UL << 63;

struct FreeListData
{
    FreeListData* pPrev {};
    FreeListData* pNext {}; /* TODO: calculate from the size (save 8 bytes) */
    u64 sizeAndIsFree {}; /* isFree bool as leftmost bit */
    u8 pMem[];

    constexpr u64 getSize() const { return sizeAndIsFree & ~IS_FREE_BITMASK; }
    constexpr bool isFree() const { return sizeAndIsFree & IS_FREE_BITMASK; }
    constexpr void setFree(bool _bFree) { _bFree ? sizeAndIsFree |= IS_FREE_BITMASK : sizeAndIsFree &= ~IS_FREE_BITMASK; };
    constexpr void setSizeSetFree(u64 _size, bool _bFree) { sizeAndIsFree = _size; setFree(_bFree); }
    constexpr void setSize(u64 _size) { setSizeSetFree(_size, isFree()); }
    constexpr void addSize(u64 _size) { setSize(_size + getSize()); }
    // constexpr FreeListData* nextNode() const { return (FreeListData*)((u8*)this + getSize()); }
};

/* best-fit logarithmic time thing */
struct FreeList
{
    using Node = RBNode<FreeListData>;

    Allocator base {};
    u64 blockSize {};
    RBTreeBase<FreeListData> tree {};
    FreeListBlock* pBlocks {};

    FreeList() = default;
    FreeList(u64 _blockSize);
};

inline void
_FreeListPrintTree(FreeList* s, Allocator* pAlloc)
{
    auto pfn = +[](const FreeList::Node* pNode, [[maybe_unused]] void* pArgs) -> void {
        CERR(
            "{}" ADT_LOGS_COL_NORM " {}\n",
            pNode->color == RB_COL::RED ? ADT_LOGS_COL_RED "(R)" : ADT_LOGS_COL_BLUE "(B)", pNode->data.getSize()
        );
    };

    RBPrintNodes(pAlloc, &s->tree, s->tree.pRoot, pfn, {}, stderr, {}, false);
}

template<>
constexpr s64
utils::compare(const FreeListData& l, const FreeListData& r)
{
    return l.getSize() - r.getSize();
}

inline FreeList::Node*
FreeListNodeFromBlock(FreeListBlock* pBlock)
{
    return (FreeList::Node*)pBlock->pMem;
}

inline FreeListBlock*
FreeListBlockFromNode(FreeList* s, FreeList::Node* pNode)
{
    auto* pBlock = s->pBlocks;
    while (pBlock)
    {
        if ((u8*)pNode > (u8*)pBlock && (u8*)pNode < (u8*)pBlock + pBlock->size)
            return pBlock;
    }

    return nullptr;
}

inline FreeListBlock*
FreeListAllocBlock(FreeList* s, u64 size)
{
    FreeListBlock* pBlock = (FreeListBlock*)::calloc(1, size);
    pBlock->size = size;

    FreeList::Node* pNode = FreeListNodeFromBlock(pBlock);
    pNode->data.setSizeSetFree(pBlock->size - sizeof(FreeListBlock) - sizeof(FreeList::Node), true);
    pNode->data.pNext = pNode->data.pPrev = nullptr;

    RBInsert(&s->tree, pNode, true);

    return pBlock;
}

inline FreeListBlock*
FreeListBlockPush(FreeList* s, u64 size)
{
    auto* pNewBlock = FreeListAllocBlock(s, size);

    auto* it = s->pBlocks;
    while (it->pNext) it = it->pNext;

    it->pNext = pNewBlock;

    return pNewBlock;
}

inline void
FreeListFreeAll(FreeList* s)
{
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
FreeListDataNodeFromPtr(void* p)
{
    return (FreeListData*)((u8*)p - sizeof(FreeListData));
}

inline FreeList::Node*
FreeListTreeNodeFromPtr(void* p)
{
    return (FreeList::Node*)((u8*)p - sizeof(FreeList::Node));
}

inline FreeList::Node*
FreeListFindFittingNode(FreeList* s, const u64 size)
{
    auto* it = s->tree.pRoot;
    const long realSize = size + sizeof(FreeList::Node);

    FreeList::Node* pLastFitting {};
    while (it)
    {
        assert(it->data.isFree() && "non free node in the free list");

        long nodeSize = it->data.getSize();

        if (nodeSize >= realSize)
            pLastFitting = it;

        /* save size for the header */
        long cmp = realSize - nodeSize;

        if (cmp == 0) break;
        else if (cmp < 0) it = it->left;
        else it = it->right;
    }

    return pLastFitting;
}

#ifndef NDEBUG
inline void
_FreeListVerify(FreeList* s)
{
    auto* pBlock = s->pBlocks;
    while (pBlock)
    {
        auto* pListNode = &FreeListNodeFromBlock(pBlock)->data;
        auto* pPrev = pListNode;

        while (pListNode)
        {
            if (pListNode->pNext)
            {
                bool bNextAdjecent = ((u8*)pListNode + pListNode->getSize()) == ((u8*)pListNode->pNext);
                assert(bNextAdjecent);
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

        pBlock = pBlock->pNext;
    }
}
#else
#define _FreeListVerify //
#endif

inline void*
FreeListAlloc(FreeList* s, u64 nMembers, u64 mSize)
{
    u64 requested = align8(nMembers * mSize);
    if (requested == 0) return nullptr;
    u64 realSize = requested + sizeof(FreeList::Node);

    /* find block that fits */
    auto* pBlock = s->pBlocks;
    while (pBlock)
    {
        bool bFits = (((long)pBlock->size - (long)sizeof(FreeListBlock)) - (long)pBlock->nBytesOccupied) >= (long)realSize;

        if (!bFits)
            pBlock = pBlock->pNext;
        else break;
    }

    if (!pBlock)
    {
again:
        pBlock = FreeListBlockPush(s, utils::max(s->blockSize, requested*2 + sizeof(FreeListBlock) + sizeof(FreeList::Node)));
    }

    auto* pFree = FreeListFindFittingNode(s, requested);
    if (!pFree) goto again;


    assert(pFree->data.isFree());

    pBlock->nBytesOccupied += realSize;
    long splitSize = long(pFree->data.getSize()) - long(realSize);

    assert(splitSize >= 0);

    RBRemove(&s->tree, pFree);

    if (splitSize <= (long)sizeof(FreeList::Node))
    {
        pFree->data.setFree(false);
        return pFree->data.pMem;
    }

    FreeList::Node* pSplit = (FreeList::Node*)((u8*)pFree + splitSize);
    pSplit->data.setSizeSetFree(realSize, false);

    pSplit->data.pNext = pFree->data.pNext;
    pSplit->data.pPrev = &pFree->data;

    if (pFree->data.pNext) pFree->data.pNext->pPrev = &pSplit->data;
    pFree->data.pNext = &pSplit->data;
    pFree->data.setSizeSetFree(splitSize, true);

    RBInsert(&s->tree, pFree, true);

    return pSplit->data.pMem;
}

inline void
FreeListFree(FreeList* s, void* ptr)
{
    auto* pThis = FreeListTreeNodeFromPtr(ptr);

    assert(!pThis->data.isFree());

    pThis->data.setFree(true);

    if (pThis->data.pNext && pThis->data.pNext->isFree())
    {
        RBRemove(&s->tree, FreeListTreeNodeFromPtr(pThis->data.pNext->pMem));

        pThis->data.addSize(pThis->data.pNext->getSize());
        if (pThis->data.pNext->pNext)
            pThis->data.pNext->pNext->pPrev = &pThis->data;
        pThis->data.pNext = pThis->data.pNext->pNext;
    }

    if (pThis->data.pPrev && pThis->data.pPrev->isFree())
    {
        auto* pPrev = FreeListTreeNodeFromPtr(pThis->data.pPrev->pMem);
        RBRemove(&s->tree, pPrev);

        pThis = pPrev;

        pThis->data.addSize(pThis->data.pNext->getSize());
        if (pThis->data.pNext->pNext)
            pThis->data.pNext->pNext->pPrev = &pThis->data;
        pThis->data.pNext = pThis->data.pNext->pNext;
    }

    RBInsert(&s->tree, pThis, true);
}

inline void*
FreeListRealloc(FreeList* s, void* ptr, u64 nMembers, u64 mSize)
{
    auto* pNode = FreeListTreeNodeFromPtr(ptr);
    long nodeSize = (long)pNode->data.getSize() - (long)sizeof(FreeList::Node);
    assert(nodeSize > 0);

    if ((long)nMembers*(long)mSize <= nodeSize) return ptr;

    assert(!pNode->data.isFree());

    auto* pRet = FreeListAlloc(s, nMembers, mSize);
    memcpy(pRet, ptr, nodeSize);
    FreeListFree(s, ptr);

    return pRet;
}

inline const AllocatorInterface inl_FreeListAllocatorVTable {
    .alloc = decltype(AllocatorInterface::alloc)(FreeListAlloc),
    .realloc = decltype(AllocatorInterface::realloc)(FreeListRealloc),
    .free = decltype(AllocatorInterface::free)(FreeListFree),
    .freeAll = decltype(AllocatorInterface::freeAll)(FreeListFreeAll),
};

inline FreeList::FreeList(u64 _blockSize)
    : base(&inl_FreeListAllocatorVTable),
      blockSize(align8(_blockSize + sizeof(FreeListBlock) + sizeof(FreeList::Node))),
      pBlocks(FreeListAllocBlock(this, this->blockSize)) {}

inline void* alloc(FreeList* s, u64 mCount, u64 mSize) { return FreeListAlloc(s, mCount, mSize); }
inline void* realloc(FreeList* s, void* p, u64 mCount, u64 mSize) { return FreeListRealloc(s, p, mCount, mSize); }
inline void free(FreeList* s, void* p) { FreeListFree(s, p); }
inline void freeAll(FreeList* s) { FreeListFreeAll(s); }

} /* namespace adt */

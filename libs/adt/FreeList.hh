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

inline void* FreeListAlloc(FreeList* s, u64 nMembers, u64 mSize);
inline void* FreeListZalloc(FreeList* s, u64 nMembers, u64 mSize);
inline void* FreeListRealloc(FreeList* s, void* ptr, u64 nMembers, u64 mSize);
inline void FreeListFree(FreeList* s, void* ptr);
inline void FreeListFreeAll(FreeList* s);

inline void* alloc(FreeList* s, u64 mCount, u64 mSize) { return FreeListAlloc(s, mCount, mSize); }
inline void* zalloc(FreeList* s, u64 mCount, u64 mSize) { return FreeListZalloc(s, mCount, mSize); }
inline void* realloc(FreeList* s, void* p, u64 mCount, u64 mSize) { return FreeListRealloc(s, p, mCount, mSize); }
inline void free(FreeList* s, void* p) { FreeListFree(s, p); }
inline void freeAll(FreeList* s) { FreeListFreeAll(s); }

struct FreeListBlock
{
    FreeListBlock* pNext {};
    u64 size {}; /* including sizeof(FreeListBlock) */
    u64 nBytesOccupied {};
    u8 pMem[];
};

struct FreeListData
{
    static constexpr u64 IS_FREE_MASK = 1ULL << 63;

    FreeListData* pPrev {};
    FreeListData* pNext {}; /* TODO: calculate from the size (save 8 bytes) */
    u64 sizeAndIsFree {}; /* isFree bool as leftmost bit */
    u8 pMem[];

    constexpr u64 getSize() const { return sizeAndIsFree & ~IS_FREE_MASK; }
    constexpr bool isFree() const { return sizeAndIsFree & IS_FREE_MASK; }
    constexpr void setFree(bool _bFree) { _bFree ? sizeAndIsFree |= IS_FREE_MASK : sizeAndIsFree &= ~IS_FREE_MASK; };
    constexpr void setSizeSetFree(u64 _size, bool _bFree) { sizeAndIsFree = _size; setFree(_bFree); }
    constexpr void setSize(u64 _size) { setSizeSetFree(_size, isFree()); }
    constexpr void addSize(u64 _size) { setSize(_size + getSize()); }
    // constexpr FreeListData* nextNode() const { return (FreeListData*)((u8*)this + getSize()); }
};

struct FreeList
{
    using Node = RBNode<FreeListData>;

    IAllocator super {};
    u64 blockSize {};
    RBTreeBase<FreeListData> tree {};
    FreeListBlock* pBlocks {};

    FreeList() = default;
    FreeList(u64 _blockSize);
};


#if defined ADT_DBG_MEMORY

inline void
_FreeListPrintTree(FreeList* s, IAllocator* pAlloc)
{
    auto pfn = +[](const FreeList::Node* pNode, [[maybe_unused]] void* pArgs) -> void {
        CERR(
            "{}" ADT_LOGS_COL_NORM " {}\n",
            pNode->color() == RB_COLOR::RED ? ADT_LOGS_COL_RED "(R)" : ADT_LOGS_COL_BLUE "(B)", pNode->data.getSize()
        );
    };

    RBPrintNodes(pAlloc, &s->tree, s->tree.pRoot, pfn, {}, stderr, {}, false);
}

#else
    #define _FreeListPrintTree(...) (void)0
#endif

template<>
constexpr long
utils::compare(const FreeListData& l, const FreeListData& r)
{
    return l.getSize() - r.getSize();
}

inline FreeList::Node*
_FreeListNodeFromBlock(FreeListBlock* pBlock)
{
    return (FreeList::Node*)pBlock->pMem;
}

inline FreeListBlock*
_FreeListBlockFromNode(FreeList* s, FreeList::Node* pNode)
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
_FreeListAllocBlock(FreeList* s, u64 size)
{
    FreeListBlock* pBlock = (FreeListBlock*)::calloc(1, size);
    pBlock->size = size;

    FreeList::Node* pNode = _FreeListNodeFromBlock(pBlock);
    pNode->data.setSizeSetFree(pBlock->size - sizeof(FreeListBlock) - sizeof(FreeList::Node), true);
    pNode->data.pNext = pNode->data.pPrev = nullptr;

    RBInsert(&s->tree, pNode, true);

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
_FreeListDataNodeFromPtr(void* p)
{
    return (FreeListData*)((u8*)p - sizeof(FreeListData));
}

inline FreeList::Node*
_FreeListTreeNodeFromPtr(void* p)
{
    return (FreeList::Node*)((u8*)p - sizeof(FreeList::Node));
}

inline FreeList::Node*
_FreeListFindFittingNode(FreeList* s, const u64 size)
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
        else if (cmp < 0) it = it->left();
        else it = it->right();
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
        auto* pListNode = &_FreeListNodeFromBlock(pBlock)->data;
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
        bool bFits = (((pdiff)pBlock->size - (pdiff)sizeof(FreeListBlock)) - (pdiff)pBlock->nBytesOccupied) >= (pdiff)realSize;

        if (!bFits)
            pBlock = pBlock->pNext;
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


    assert(pFree->data.isFree());

    pBlock->nBytesOccupied += realSize;
    s64 splitSize = s64(pFree->data.getSize()) - s64(realSize);

    assert(splitSize >= 0);

    RBRemove(&s->tree, pFree);

    if (splitSize <= (s64)sizeof(FreeList::Node))
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

inline void*
FreeListZalloc(FreeList* s, u64 nMembers, u64 mSize)
{
    auto* p = FreeListAlloc(s, nMembers, mSize);
    memset(p, 0, nMembers * mSize);
    return p;
}

inline void
FreeListFree(FreeList* s, void* ptr)
{
    auto* pThis = _FreeListTreeNodeFromPtr(ptr);

    assert(!pThis->data.isFree());

    pThis->data.setFree(true);

    /* next adjecent node coalescence */
    if (pThis->data.pNext && pThis->data.pNext->isFree())
    {
        RBRemove(&s->tree, _FreeListTreeNodeFromPtr(pThis->data.pNext->pMem));

        pThis->data.addSize(pThis->data.pNext->getSize());
        if (pThis->data.pNext->pNext)
            pThis->data.pNext->pNext->pPrev = &pThis->data;
        pThis->data.pNext = pThis->data.pNext->pNext;
    }

    /* prev adjecent node coalescence */
    if (pThis->data.pPrev && pThis->data.pPrev->isFree())
    {
        auto* pPrev = _FreeListTreeNodeFromPtr(pThis->data.pPrev->pMem);
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
    if (!ptr) return FreeListAlloc(s, nMembers, mSize);

    auto* pNode = _FreeListTreeNodeFromPtr(ptr);
    s64 nodeSize = (s64)pNode->data.getSize() - (s64)sizeof(FreeList::Node);
    assert(nodeSize > 0);

    if ((s64)nMembers*(s64)mSize <= nodeSize) return ptr;

    assert(!pNode->data.isFree());

    auto* pRet = FreeListAlloc(s, nMembers, mSize);
    memcpy(pRet, ptr, nodeSize);
    FreeListFree(s, ptr);

    return pRet;
}

inline const AllocatorVTable inl_FreeListAllocatorVTable {
    .alloc = decltype(AllocatorVTable::alloc)(FreeListAlloc),
    .zalloc = decltype(AllocatorVTable::zalloc)(FreeListZalloc),
    .realloc = decltype(AllocatorVTable::realloc)(FreeListRealloc),
    .free = decltype(AllocatorVTable::free)(FreeListFree),
    .freeAll = decltype(AllocatorVTable::freeAll)(FreeListFreeAll),
};

inline FreeList::FreeList(u64 _blockSize)
    : super(&inl_FreeListAllocatorVTable),
      blockSize(align8(_blockSize + sizeof(FreeListBlock) + sizeof(FreeList::Node))),
      pBlocks(_FreeListAllocBlock(this, this->blockSize)) {}

} /* namespace adt */

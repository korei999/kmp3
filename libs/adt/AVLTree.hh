#pragma once

#include "Allocator.hh"
#include "String.hh"
#include "utils.hh"
#include "Pair.hh"

#include <cstdio>
#include <cassert>

namespace adt
{

enum class AVL_ORDER { PRE, IN, POST };

template<typename T>
struct AVLNode
{
    AVLNode* pParent = nullptr;
    AVLNode* pLeft = nullptr;
    AVLNode* pRight = nullptr;
    s16 height = 0;
    T data {};
};

template<typename T>
struct AVLTree
{
    Allocator* pAlloc = nullptr;
    AVLNode<T>* pRoot = nullptr;

    AVLTree() = default;
    AVLTree(Allocator* pA) : pAlloc {pA} {}
};

template<typename T> AVLNode<T>* AVLNodeAlloc(Allocator* pA, const T& data);
template<typename T> inline AVLNode<T>* AVLMin(AVLNode<T>* p);
template<typename T> inline AVLNode<T>* AVLMax(AVLNode<T>* p);
template<typename T> inline void AVLRemove(AVLTree<T>* s, AVLNode<T>* d);
template<typename T> inline AVLNode<T>* AVLInsert(AVLTree<T>* s, AVLNode<T>* pNew, bool bAllowDupticates);
template<typename T> inline AVLNode<T>* AVLInsert(AVLTree<T>* s, const T& data, bool bAllowDupticates);

template<typename T>
inline Pair<AVLNode<T>*, AVLNode<T>*>
AVLTraverse(
    AVLNode<T>* parent,
    AVLNode<T>* p,
    bool (*pfn)(AVLNode<T>*, AVLNode<T>*, void*),
    void* pUserData,
    enum AVL_ORDER order
);

template<typename T> inline AVLNode<T>* AVLSearch(AVLNode<T>* p, const T& data);
template<typename T> inline s16 AVLDepth(AVLNode<T>* p);
template<typename T> inline s16 AVLNodeHeight(AVLNode<T>* p);

template<typename T>
inline void
AVLPrintNodes(
    Allocator* pA,
    const AVLTree<T>* s,
    const AVLNode<T>* pNode,
    void (*pfnPrint)(const AVLNode<T>*, void*),
    void* pFnData,
    FILE* pF,
    const String sPrefix,
    bool bLeft
);

template<typename T> inline void AVLDestroy(AVLTree<T>* s);

template<typename T> inline s16 __AVLNodeBalance(AVLNode<T>* p);
template<typename T> inline void __AVLUpdateHeight(AVLNode<T>* p);
template<typename T> inline AVLNode<T>* __AVLRightRotate(AVLTree<T>* s, AVLNode<T>* a);
template<typename T> inline AVLNode<T>* __AVLLeftRotate(AVLTree<T>* s, AVLNode<T>* b);
template<typename T> inline AVLNode<T>* __AVLLeftLeftCase(AVLTree<T>* s, AVLNode<T>* node);
template<typename T> inline AVLNode<T>* __AVLLeftRightCase(AVLTree<T>* s, AVLNode<T>* p);
template<typename T> inline AVLNode<T>* __AVLRightRightCase(AVLTree<T>* s, AVLNode<T>* node);
template<typename T> inline AVLNode<T>* __AVLRightLeftCase(AVLTree<T>* s, AVLNode<T>* p);
template<typename T> inline void __AVLTransplant(AVLTree<T>* s, AVLNode<T>* u, AVLNode<T>* v);
template<typename T> inline void __AVLRebalance(AVLTree<T>* s, AVLNode<T>* p);

template<typename T>
AVLNode<T>*
AVLNodeAlloc(Allocator* pA, const T& data)
{
    auto* pNew = (AVLNode<T>*)alloc(pA, 1, sizeof(AVLNode<T>));
    pNew->data = data;
    return pNew;
}

template<typename T>
inline s16
AVLNodeHeight(AVLNode<T>* p)
{
    return (p ? p->height : -1);
}

template<typename T>
inline s16
__AVLNodeBalance(AVLNode<T>* p)
{
    return AVLNodeHeight(p->pLeft) - AVLNodeHeight(p->pRight);
}

template<typename T>
inline void
__AVLUpdateHeight(AVLNode<T>* p)
{
    s16 lh = AVLNodeHeight(p->pLeft);
    s16 rh = AVLNodeHeight(p->pRight);
    p->height = 1 + utils::max(lh, rh);
}

/*            (P)                    (P)
 *             |                      |
 *            (A)    RightRotate     (B)
 *           /  \    ---------->    /  \
 *         (B)  (C)               (D)  (A)
 *        /  \                        /  \
 *      (D)  (E)                    (E)  (C)     */
template<typename T>
inline AVLNode<T>*
__AVLRightRotate(AVLTree<T>* s, AVLNode<T>* a)
{
    auto p = a->pParent;
    auto b = a->pLeft;

    a->pLeft = b->pRight;

    if (b->pRight) b->pRight->pParent = a;

    b->pRight = a;
    a->pParent = b;
    b->pParent = p;

    if (p)
    {
        if (p->pLeft == a) p->pLeft = b;
        else p->pRight = b;
    } else s->pRoot = b;

    __AVLUpdateHeight(b);
    __AVLUpdateHeight(a);
    return b;
}

/*         (P)                      (P)
 *          |                        |
 *         (B)     LeftRotate       (A)
 *        /  \     --------->      /  \
 *      (D)  (A)                 (B)  (C)
 *          /  \                /  \
 *        (E)  (C)            (D)  (E)          */
template<typename T>
inline AVLNode<T>*
__AVLLeftRotate(AVLTree<T>* s, AVLNode<T>* b)
{
    auto p = b->pParent;
    auto a = b->pRight;

    b->pRight = a->pLeft;

    if (a->pLeft) a->pLeft->pParent = b;

    a->pLeft = b;
    b->pParent = a;
    a->pParent = p;

    if (p)
    {
        if (p->pLeft == b) p->pLeft = a;
        else p->pRight = a;
    } else s->pRoot = a;

    __AVLUpdateHeight(a);
    __AVLUpdateHeight(b);
    return a;
}

template<typename T>
inline AVLNode<T>*
__AVLLeftLeftCase(AVLTree<T>* s, AVLNode<T>* node)
{
    return __AVLRightRotate(s, node);
}

template<typename T>
inline AVLNode<T>*
__AVLLeftRightCase(AVLTree<T>* s, AVLNode<T>* p)
{
    p->pLeft = __AVLLeftRotate(s, p->pLeft);
    return __AVLRightRotate(s, p);
}

template<typename T>
inline AVLNode<T>*
__AVLRightRightCase(AVLTree<T>* s, AVLNode<T>* node)
{
    return __AVLLeftRotate(s, node);
}

template<typename T>
inline AVLNode<T>*
__AVLRightLeftCase(AVLTree<T>* s, AVLNode<T>* p)
{
    p->pRight = __AVLRightRotate(s, p->pRight);
    return __AVLLeftRotate(s, p);
}

template<typename T>
inline AVLNode<T>*
AVLMin(AVLNode<T>* p)
{
    assert(p && "Min of nullptr");
    while (p->pLeft) p = p->pLeft;
    return p;
}

template<typename T>
inline AVLNode<T>*
AVLMax(AVLNode<T>* p)
{
    assert(p && "Max of nullptr");
    while (p->pRight) p = p->pRight;
    return p;
}

template<typename T>
inline void
__AVLTransplant(AVLTree<T>* s, AVLNode<T>* u, AVLNode<T>* v)
{
    if (!u->pParent) s->pRoot = v;
    else if (u == u->pParent->pLeft) u->pParent->pLeft = v;
    else u->pParent->pRight = v;

    if (v) v->pParent = u->pParent;
}

template<typename T>
inline void
__AVLRebalance(AVLTree<T>* s, AVLNode<T>* p)
{
    while (p)
    {
        s16 diff = __AVLNodeBalance(p);

        if (diff <= -2)
        {
            if (AVLNodeHeight(p->pRight->pRight) < AVLNodeHeight(p->pRight->pLeft))
                p = __AVLRightLeftCase(s, p);
            else p = __AVLRightRightCase(s, p);

        }
        else if (diff >= 2)
        {
            if (AVLNodeHeight(p->pLeft->pLeft) < AVLNodeHeight(p->pLeft->pRight))
                p = __AVLLeftRightCase(s, p);
            else p = __AVLLeftLeftCase(s, p);
        }

        __AVLUpdateHeight(p);
        p = p->pParent;
    }
}

template<typename T>
inline void
AVLRemove(AVLTree<T>* s, AVLNode<T>* d)
{
    AVLNode<T>* succ, * toBalance;

    assert(d && "removing nullptr");

    /* root is the single element case */
    if (!d->pParent && !d->pRight && !d->pLeft)
    {
        /*free(s->pAlloc, d);*/
        s->pRoot = nullptr;
        return;
    }

    if (!d->pLeft)
    {
        toBalance = d->pParent;
        __AVLTransplant(s, d, d->pRight);

        if (!toBalance) toBalance = d->pRight;

        /*free(s->pAlloc, d);*/
    }
    else if (!d->pRight)
    {
        toBalance = d->pParent;
        __AVLTransplant(s, d, d->pLeft);

        if (!toBalance) toBalance = d->pLeft;

        /*free(s->pAlloc, d);*/
    }
    else
    {
        succ = AVLMin(d->pRight);

        if (succ->pParent != d)
        {
            toBalance = succ->pParent;
            __AVLTransplant(s, succ, succ->pRight);
            succ->pRight = d->pRight;
            succ->pRight->pParent = succ;
        }
        else
        {
            if (succ->pRight) toBalance = succ->pRight;
            else toBalance = succ;
        }

        __AVLTransplant(s, d, succ);

        succ->pLeft = d->pLeft;
        succ->pLeft->pParent = succ;

        /*free(s->pAlloc, d);*/
    }

    __AVLUpdateHeight(toBalance);
    __AVLRebalance(s, toBalance);

    /* FIXME: add real free */
    /*free(s->pAlloc, d);*/
}

template<typename T>
inline AVLNode<T>*
AVLInsert(AVLTree<T>* s, AVLNode<T>* pNew, bool bAllowDupticates)
{
    AVLNode<T>** ppSelf = &s->pRoot, * pParent = nullptr;
    s64 comp = 0;
    while (true)
    {
        AVLNode<T>* n = *ppSelf;
        if (!n)
        {
            pNew->height = 0;
            pNew->pParent = pParent;
            pNew->pLeft = pNew->pRight = nullptr;
            *ppSelf = pNew;
            break;
        }

        comp = utils::compare(pNew->data, n->data);

        if (comp == 0)
        {
            if (bAllowDupticates)
            {
                /* left case */
                pParent = *ppSelf;
                ppSelf = &n->pLeft;
            }
            else return n;
        }
        else if (comp < 0)
        {
            pParent = *ppSelf;
            ppSelf = &n->pLeft;
        }
        else
        {
            pParent = *ppSelf;
            ppSelf = &n->pRight;
        }
    }

    __AVLRebalance(s, pNew);
    return pNew;
}

template<typename T>
inline AVLNode<T>*
AVLInsert(AVLTree<T>* s, const T& data, bool bAllowDupticates)
{
    auto* pNew = AVLNodeAlloc(s->pAlloc, data);
    return AVLInsert(s, pNew, bAllowDupticates);
}

/* early return if pfn returns true */
template<typename T>
inline Pair<AVLNode<T>*, AVLNode<T>*>
AVLTraverse(
    AVLNode<T>* parent,
    AVLNode<T>* p,
    bool (*pfn)(AVLNode<T>*, AVLNode<T>*, void*),
    void* pUserData,
    enum AVL_ORDER order
)
{
    if (p)
    {
        switch (order)
        {
            case AVL_ORDER::PRE:
                if (pfn(parent, p, pUserData)) return {parent, p};
                AVLTraverse(p, p->pLeft, pfn, pUserData, order);
                AVLTraverse(p, p->pRight, pfn, pUserData, order);
                break;
            case AVL_ORDER::IN:
                AVLTraverse(p, p->pLeft, pfn, pUserData, order);
                if (pfn(parent, p, pUserData)) return {parent, p};
                AVLTraverse(p, p->pRight, pfn, pUserData, order);
                break;
            case AVL_ORDER::POST:
                AVLTraverse(p, p->pLeft, pfn, pUserData, order);
                AVLTraverse(p, p->pRight, pfn, pUserData, order);
                if (pfn(parent, p, pUserData)) return {parent, p};
                break;
        }
    }

    return {};
}

template<typename T>
inline AVLNode<T>*
AVLSearch(AVLNode<T>* p, const T& data)
{
    if (p)
    {
        if (data == p->data) return p;
        else if (data < p->data) return AVLSearch(p->pLeft, data);
        else return AVLSearch(p->pRight, data);
    } else return nullptr;
}

template<typename T>
inline s16
AVLDepth(AVLNode<T>* p)
{
    if (p)
    {
        s16 l = AVLDepth(p->pLeft);
        s16 r = AVLDepth(p->pRight);
        return 1 + utils::max(l, r);
    } else return 0;
}

template<typename T>
inline void
AVLPrintNodes(
    Allocator* pA,
    const AVLTree<T>* s,
    const AVLNode<T>* pNode,
    void (*pfnPrint)(const AVLNode<T>*, void*),
    void* pFnData,
    FILE* pF,
    const String sPrefix,
    bool bLeft
)
{
    if (pNode)
    {
        fprintf(pF, "%.*s%s", sPrefix.size, sPrefix.pData, bLeft ? "|__" : "\\__");
        pfnPrint(pNode, pFnData);

        String sCat = StringCat(pA, sPrefix, bLeft ? "|   " : "    ");

        AVLPrintNodes(pA, s, pNode->pLeft, pfnPrint, pFnData, pF, sCat, true);
        AVLPrintNodes(pA, s, pNode->pRight, pfnPrint, pFnData, pF, sCat, false);
    }
}

template<typename T>
inline void
AVLDestroy(AVLTree<T>* s)
{
    auto pfnFree = +[]([[maybe_unused]] AVLNode<T>* pPar, AVLNode<T>* p, void* data) -> bool {
        free(((AVLTree<T>*)data)->pAlloc, p);

        return false;
    };

    AVLTraverse({}, s->pRoot, pfnFree, s, AVL_ORDER::POST);
}

} /* namespace adt */

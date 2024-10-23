/* Some code is borrowed from OpenBSD's red-black tree implementation. */

/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Allocator.hh"
#include "String.hh"
#include "utils.hh"
#include "Pair.hh"

#include <cstdio>

namespace adt
{

enum class RB_COL : u8 { BLACK, RED };
enum class RB_ORDER { PRE, IN, POST };

template<typename T>
struct RBNode
{
    RBNode* left;
    RBNode* right;
    RBNode* parent;
    enum RB_COL color;
    T data;
};

template<typename T>
struct RBTreeBase
{
    RBNode<T>* pRoot = nullptr;
};

template<typename T> inline RBNode<T>* RBRoot(RBTreeBase<T>* s);
template<typename T> inline RBNode<T>* RBNodeAlloc(Allocator* pA, const T& data);
template<typename T> inline bool RBEmpty(RBTreeBase<T>* s);
template<typename T> inline RBNode<T>* RBRemove(RBTreeBase<T>* s, RBNode<T>* elm);
template<typename T> inline void RBRemoveAndFree(RBTreeBase<T>* s, Allocator* p, RBNode<T>* elm);
template<typename T> inline RBNode<T>* RBInsert(RBTreeBase<T>* s, RBNode<T>* elm, bool bAllowDuplicates);
template<typename T> inline RBNode<T>* RBInsert(RBTreeBase<T>* s, Allocator* pA, const T& data, bool bAllowDuplicates);

template<typename T>
inline Pair<RBNode<T>*, RBNode<T>*>
RBTraverse(
    RBNode<T>* parent,
    RBNode<T>* p,
    bool (*pfn)(RBNode<T>*, RBNode<T>*, void*),
    void* pUserData,
    enum RB_ORDER order
);

template<typename T> inline RBNode<T>* RBSearch(RBNode<T>* p, const T& data);
template<typename T> inline int RBDepth(RBNode<T>* p);

template<typename T>
inline void
RBPrintNodes(
    Allocator* pA,
    const RBTreeBase<T>* s,
    const RBNode<T>* pNode,
    void (*pfnPrint)(const RBNode<T>*, void*),
    void* pFnData,
    FILE* pF,
    const String sPrefix,
    bool bLeft
);

template<typename T> inline void RBDestroy(RBTreeBase<T>* s);

template<typename T> inline void __RBSetBlackRed(RBNode<T>* black, RBNode<T>* red);
template<typename T> inline void __RBSet(RBNode<T>* elm, RBNode<T>* parent);
template<typename T> inline void __RBSetLinks(RBNode<T>* l, RBNode<T>* r);
template<typename T> inline void __RBRotateLeft(RBTreeBase<T>* s, RBNode<T>* elm);
template<typename T> inline void __RBRotateRight(RBTreeBase<T>* s, RBNode<T>* elm);
template<typename T> inline void __RBInsertColor(RBTreeBase<T>* s, RBNode<T>* elm);
template<typename T> inline void __RBRemoveColor(RBTreeBase<T>* s, RBNode<T>* parent, RBNode<T>* elm);

template<typename T>
inline void
__RBSetLinks(RBNode<T>* l, RBNode<T>* r)
{
    l->left = r->left;
    l->right = r->right;
    l->parent = r->parent;
    l->color = r->color;
}

template<typename T>
inline void
__RBSet(RBNode<T>* elm, RBNode<T>* parent)
{
    elm->parent = parent;
    elm->left = elm->right = nullptr;
    elm->color = RB_COL::RED;
}

template<typename T>
inline void
__RBSetBlackRed(RBNode<T>* black, RBNode<T>* red)
{
    black->color = RB_COL::BLACK;
    red->color = RB_COL::RED;
}

template<typename T>
inline RBNode<T>*
RBRoot(RBTreeBase<T>* s)
{
    return s->pRoot;
}

template<typename T>
inline RBNode<T>*
RBNodeAlloc(Allocator* pA, const T& data)
{
    auto* r = (RBNode<T>*)alloc(pA, 1, sizeof(RBNode<T>));
    r->data = data;
    return r;
}

template<typename T>
inline bool
RBEmpty(RBTreeBase<T>* s)
{
    return s->pRoot;
}

template<typename T>
inline void
__RBRotateLeft(RBTreeBase<T>* s, RBNode<T>* elm)
{
    auto tmp = elm->right;
    if ((elm->right = tmp->left))
    {
        tmp->left->parent = elm;
    }
    if ((tmp->parent = elm->parent))
    {
        if (elm == elm->parent->left)
            elm->parent->left = tmp;
        else
            elm->parent->right = tmp;
    }
    else
        s->pRoot = tmp;

    tmp->left = elm;
    elm->parent = tmp;
}

template<typename T>
inline void
__RBRotateRight(RBTreeBase<T>* s, RBNode<T>* elm)
{
    auto tmp = elm->left;
    if ((elm->left = tmp->right))
    {
        tmp->right->parent = elm;
    }
    if ((tmp->parent = elm->parent))
    {
        if (elm == elm->parent->left)
            elm->parent->left = tmp;
        else
            elm->parent->right = tmp;
    }
    else
        s->pRoot = tmp;

    tmp->right = elm;
    elm->parent = tmp;
}

template<typename T>
inline void
__RBInsertColor(RBTreeBase<T>* s, RBNode<T>* elm)
{
    RBNode<T>* parent, * gparent, * tmp;
    while ((parent = elm->parent) && parent->color == RB_COL::RED)
    {
        gparent = parent->parent;
        if (parent == gparent->left)
        {
            tmp = gparent->right;
            if (tmp && tmp->color == RB_COL::RED)
            {
                tmp->color = RB_COL::BLACK;
                __RBSetBlackRed(parent, gparent);
                elm = gparent;
                continue;
            }
            if (parent->right == elm)
            {
                __RBRotateLeft(s, parent);
                tmp = parent;
                parent = elm;
                elm = tmp;
            }
            __RBSetBlackRed(parent, gparent);
            __RBRotateRight(s, gparent);
        }
        else
        {
            tmp = gparent->left;
            if (tmp && tmp->color == RB_COL::RED)
            {
                tmp->color = RB_COL::BLACK;
                __RBSetBlackRed(parent, gparent);
                elm = gparent;
                continue;
            }
            if (parent->left == elm)
            {
                __RBRotateRight(s, parent);
                tmp = parent;
                parent = elm;
                elm = tmp;
            }
            __RBSetBlackRed(parent, gparent);
            __RBRotateLeft(s, gparent);
        }
    }
    s->pRoot->color = RB_COL::BLACK;
}

template<typename T>
inline void
__RBRemoveColor(RBTreeBase<T>* s, RBNode<T>* parent, RBNode<T>* elm)
{
    RBNode<T>* tmp;
    while ((elm == nullptr || elm->color == RB_COL::BLACK) && elm != s->pRoot)
    {
        if (parent->left == elm)
        {
            tmp = parent->right;
            if (tmp->color == RB_COL::RED)
            {
                __RBSetBlackRed(tmp, parent);
                __RBRotateLeft(s, parent);
                tmp = parent->right;
            }
            if ((tmp->left == nullptr || tmp->left->color == RB_COL::BLACK) &&
                (tmp->right == nullptr || tmp->right->color == RB_COL::BLACK))
            {
                tmp->color = RB_COL::RED;
                elm = parent;
                parent = elm->parent;
            }
            else
            {
                if (tmp->right == nullptr || tmp->right->color == RB_COL::BLACK)
                {
                    RBNode<T>* oleft;
                    if ((oleft = tmp->left))
                        oleft->color = RB_COL::BLACK;
                    tmp->color = RB_COL::RED;
                    __RBRotateRight(s, tmp);
                    tmp = parent->right;
                }
                tmp->color = parent->color;
                parent->color = RB_COL::BLACK;
                if (tmp->right)
                    tmp->right->color = RB_COL::BLACK;
                __RBRotateLeft(s, parent);
                elm = s->pRoot;
                break;
            }
        }
        else
        {
            tmp = parent->left;
            if (tmp->color == RB_COL::RED)
            {
                __RBSetBlackRed(tmp, parent);
                __RBRotateRight(s, parent);
                tmp = parent->left;
            }
            if ((tmp->left == nullptr || tmp->left->color == RB_COL::BLACK) &&
                (tmp->right == nullptr || tmp->right->color == RB_COL::BLACK))
            {
                tmp->color = RB_COL::RED;
                elm = parent;
                parent = elm->parent;
            }
            else
            {
                if (tmp->left == nullptr || tmp->left->color == RB_COL::BLACK)
                {
                    RBNode<T>* oright;
                    if ((oright = tmp->right))
                        oright->color = RB_COL::BLACK;
                    tmp->color = RB_COL::RED;
                    __RBRotateLeft(s, tmp);
                    tmp = parent->left;
                }
                tmp->color = parent->color;
                parent->color = RB_COL::BLACK;
                if (tmp->left)
                    tmp->left->color = RB_COL::BLACK;
                __RBRotateRight(s, parent);
                elm = s->pRoot;
                break;
            }
        }
    }
    if (elm)
        elm->color = RB_COL::BLACK;
}

template<typename T>
inline RBNode<T>*
RBRemove(RBTreeBase<T>* s, RBNode<T>* elm)
{
    RBNode<T>* child, * parent, * old = elm;
    enum RB_COL color;
    if (elm->left == nullptr)
        child = elm->right;
    else if (elm->right == nullptr)
        child = elm->left;
    else
    {
        RBNode<T>* left;
        elm = elm->right;
        while ((left = elm->left))
            elm = left;
        child = elm->right;
        parent = elm->parent;
        color = elm->color;
        if (child)
            child->parent = parent;
        if (parent)
        {
            if (parent->left == elm)
                parent->left = child;
            else
                parent->right = child;
        }
        else
            s->pRoot = child;
        if (elm->parent == old)
            parent = elm;

        __RBSetLinks(elm, old);

        if (old->parent)
        {
            if (old->parent->left == old)
                old->parent->left = elm;
            else
                old->parent->right = elm;
        }
        else
            s->pRoot = elm;
        old->left->parent = elm;
        if (old->right)
            old->right->parent = elm;
        goto color;
    }
    parent = elm->parent;
    color = elm->color;
    if (child)
        child->parent = parent;
    if (parent)
    {
        if (parent->left == elm)
            parent->left = child;
        else
            parent->right = child;
    }
    else
        s->pRoot = child;
color:
    if (color == RB_COL::BLACK)
        __RBRemoveColor(s, parent, child);

    return (old);
}

template<typename T>
inline void
RBRemoveAndFree(RBTreeBase<T>* s, Allocator* p, RBNode<T>* elm)
{
    free(p, RBRemove(s, elm));
}

/* create RBNode outside then insert */
template<typename T>
inline RBNode<T>*
RBInsert(RBTreeBase<T>* s, RBNode<T>* elm, bool bAllowDuplicates)
{
    RBNode<T>* tmp;
    RBNode<T>* parent = nullptr;
    s64 comp = 0;
    tmp = s->pRoot;
    while (tmp)
    {
        parent = tmp;
        comp = utils::compare(elm->data, parent->data);

        if (comp == 0)
        {
            /* left case */
            if (bAllowDuplicates) tmp = tmp->left;
            else return tmp;
        }
        else if (comp < 0) tmp = tmp->left;
        else tmp = tmp->right;
    }

    __RBSet(elm, parent);

    if (parent != nullptr)
    {
        if (comp <= 0) parent->left = elm;
        else parent->right = elm;
    }
    else s->pRoot = elm;

    __RBInsertColor(s, elm);
    return elm;
}

template<typename T>
inline RBNode<T>*
RBInsert(RBTreeBase<T>* s, Allocator* pA, const T& data, bool bAllowDuplicates)
{
    RBNode<T>* pNew = RBNodeAlloc(pA, data);
    return RBInsert(s, pNew, bAllowDuplicates);
}

/* early return if pfn returns true */
template<typename T>
inline Pair<RBNode<T>*, RBNode<T>*>
RBTraverse(
    RBNode<T>* parent,
    RBNode<T>* p,
    bool (*pfn)(RBNode<T>*, RBNode<T>*, void*),
    void* pUserData,
    enum RB_ORDER order
)
{
    if (p)
    {
        switch (order)
        {
            case RB_ORDER::PRE:
                if (pfn(parent, p, pUserData)) return {parent, p};
                RBTraverse(p, p->left, pfn, pUserData, order);
                RBTraverse(p, p->right, pfn, pUserData, order);
                break;

            case RB_ORDER::IN:
                RBTraverse(p, p->left, pfn, pUserData, order);
                if (pfn(parent, p, pUserData)) return {parent, p};
                RBTraverse(p, p->right, pfn, pUserData, order);
                break;

            case RB_ORDER::POST:
                RBTraverse(p, p->left, pfn, pUserData, order);
                RBTraverse(p, p->right, pfn, pUserData, order);
                if (pfn(parent, p, pUserData)) return {parent, p};
                break;
        }
    }

    return {};
}

template<typename T>
inline RBNode<T>*
RBSearch(RBNode<T>* p, const T& data)
{
    if (p)
    {
        if (data == p->data) return p;
        else if (data < p->data) return RBSearch(p->left, data);
        else return RBSearch(p->right, data);
    } else return nullptr;
}

template<typename T>
inline int
RBDepth(RBNode<T>* p)
{
    if (p)
    {
        int l = RBDepth(p->left);
        int r = RBDepth(p->right);
        return 1 + utils::max(l, r);
    } else return 0;
}

template<typename T>
inline void
RBPrintNodes(
    Allocator* pA,
    const RBTreeBase<T>* s,
    const RBNode<T>* pNode,
    void (*pfnPrint)(const RBNode<T>*, void*),
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

        RBPrintNodes(pA, s, pNode->left, pfnPrint, pFnData, pF, sCat, true);
        RBPrintNodes(pA, s, pNode->right, pfnPrint, pFnData, pF, sCat, false);

        free(pA, sCat.pData);
    }
}

template<typename T>
inline void
RBDestroy(RBTreeBase<T>* s)
{
    auto pfnFree = +[]([[maybe_unused]] RBNode<T>* pPar, RBNode<T>* p, void* data) -> bool {
        free(((RBTreeBase<T>*)data)->pAlloc, p);

        return false;
    };

    RBTraverse(nullptr, s->pRoot, pfnFree, s, RB_ORDER::POST);
}

template<typename T>
struct RBTree
{
    RBTreeBase<T> base {};
    Allocator* pA {};
    
    RBTree() = default;
    RBTree(Allocator* p) : pA(p) {}
};

template<typename T>
inline RBNode<T>*
RBRoot(RBTree<T>* s)
{
    return RBRoot(&s->base);
}

template<typename T>
inline bool
RBEmpty(RBTree<T>* s)
{
    return RBEmpty(&s->base);
}

template<typename T>
inline RBNode<T>*
RBRemove(RBTree<T>* s, RBNode<T>* elm)
{
    return RBRemove(&s->base, elm);
}

template<typename T>
inline RBNode<T>*
RBRemove(RBTree<T>* s, const T& x)
{
    return RBRemove(&s->base, RBSearch(s->base.pRoot, x));
}

template<typename T>
inline void
RBRemoveAndFree(RBTree<T>* s, RBNode<T>* elm)
{
    RBRemoveAndFree(&s->base, s->pA, elm);
}

template<typename T>
inline void
RBRemoveAndFree(RBTree<T>* s, const T& x)
{
    RBRemoveAndFree(&s->base, s->pA, RBSearch(s->base.pRoot, x));
}

template<typename T>
inline RBNode<T>*
RBInsert(RBTree<T>* s, RBNode<T>* elm, bool bAllowDuplicates)
{
    return RBInsert(&s->base, elm, bAllowDuplicates);
}

template<typename T>
inline RBNode<T>*
RBInsert(RBTree<T>* s, const T& data, bool bAllowDuplicates)
{
    return RBInsert(&s->base, s->pA, data, bAllowDuplicates);
}

} /* namespace adt */

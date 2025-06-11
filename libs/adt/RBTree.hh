/* Borrowed from OpenBSD's red-black tree implementation. */

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

#include "IAllocator.hh"
#include "String.hh"
#include "utils.hh"
#include "logs.hh"

#include <cstdio>

#ifdef _WIN32
    #undef IN
#endif

namespace adt
{

enum class RB_COLOR : u8 { BLACK, RED };
enum class RB_ORDER : u8 { PRE, IN, POST };

template<typename T>
struct RBNode
{
    static constexpr usize COLOR_MASK = 1ULL;

    /* */

    RBNode* m_left {};
    RBNode* m_right {};
    RBNode* m_parentColor {}; /* NOTE: color is stored as the least significant bit */
    ADT_NO_UNIQUE_ADDRESS T m_data {};

    /* */

    T& data() { return m_data; }
    const T& data() const { return m_data; }

    RBNode*& left() { return m_left; }
    RBNode* const& left() const { return m_left; }
    RBNode*& right() { return m_right; }
    RBNode* const& right() const { return m_right; }

    RB_COLOR color() const { return (RB_COLOR)((usize)m_parentColor & COLOR_MASK); }
    RB_COLOR setColor(const RB_COLOR eColor) { m_parentColor = (RBNode*)((usize)parent() | (usize)eColor); return eColor; }

    RBNode* parent() const {  return (RBNode*)((usize)m_parentColor & ~COLOR_MASK); }
    void setParent(RBNode* par) { m_parentColor = (RBNode*)(((usize)par & ~COLOR_MASK) | (usize)color()); }

    RBNode*& parentAndColor() { return m_parentColor; }
    RBNode* const& parentAndColor() const { return m_parentColor; }
};

template<typename T>
[[nodiscard]] inline RBNode<T>* RBNodeAlloc(IAllocator* pA, const T& data);

template<typename T, typename ...ARGS>
[[nodiscard]] inline RBNode<T>* RBNodeAlloc(IAllocator* pA, ARGS&& ...args);

template<typename T>
inline RBNode<T>* RBTraversePRE(
    RBNode<T>* p,
    bool (*pfn)(RBNode<T>* pNode, void* pArg),
    void* pUserData
);

template<typename T>
inline RBNode<T>* RBTraverseIN(
    RBNode<T>* p,
    bool (*pfn)(RBNode<T>* pNode, void* pArg),
    void* pUserData
);

template<typename T>
inline RBNode<T>* RBTraversePOST(
    RBNode<T>* p,
    bool (*pfn)(RBNode<T>* pNode, void* pArg),
    void* pUserData
);

template<typename T>
inline RBNode<T>* RBTraverse(
    RBNode<T>* p,
    bool (*pfn)(RBNode<T>* pNode, void* pArg),
    void* pUserData,
    RB_ORDER order
);

template<typename T>
inline RBNode<T>* RBSearch(RBNode<T>* p, const T& data);

template<typename T>
inline int RBDepth(RBNode<T>* p);

template<typename T>
struct RBTree
{
    RBNode<T>* m_pRoot = nullptr;
    usize m_size = 0;

    RBNode<T>* root();
    bool empty();
    RBNode<T>* remove(RBNode<T>* elm);
    void removeAndFree(IAllocator* p, RBNode<T>* elm);
    void removeAndFree(IAllocator* p, const T& elm);
    RBNode<T>* insert(bool bAllowDuplicates, RBNode<T>* elm);
    RBNode<T>* insert(IAllocator* pA, bool bAllowDuplicates, const T& data);
    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>) RBNode<T>* emplace(IAllocator* pA, bool bAllowDuplicates, ARGS&&... args);
    void destroy(IAllocator* pA);
};

template<typename T>
inline void
RBPrintNodes(
    IAllocator* pA,
    const RBNode<T>* pNode,
    FILE* pF,
    const StringView sPrefix = "",
    bool bLeft = false
);

template<typename T> inline void _RBSetBlackRed(RBNode<T>* black, RBNode<T>* red);
template<typename T> inline void _RBSet(RBNode<T>* elm, RBNode<T>* parent);
template<typename T> inline void _RBSetLinks(RBNode<T>* l, RBNode<T>* r);
template<typename T> inline void _RBRotateLeft(RBTree<T>* s, RBNode<T>* elm);
template<typename T> inline void _RBRotateRight(RBTree<T>* s, RBNode<T>* elm);
template<typename T> inline void _RBInsertColor(RBTree<T>* s, RBNode<T>* elm);
template<typename T> inline void _RBRemoveColor(RBTree<T>* s, RBNode<T>* parent, RBNode<T>* elm);

template<typename T>
inline void
_RBSetLinks(RBNode<T>* l, RBNode<T>* r)
{
    l->left() = r->left();
    l->right() = r->right();
    l->parentAndColor() = r->parentAndColor();
}

template<typename T>
inline void
_RBSet(RBNode<T>* elm, RBNode<T>* parent)
{
    elm->setParent(parent);
    elm->left() = elm->right() = nullptr;
    elm->setColor(RB_COLOR::RED);
}

template<typename T>
inline void
_RBSetBlackRed(RBNode<T>* black, RBNode<T>* red)
{
    black->setColor(RB_COLOR::BLACK);
    red->setColor(RB_COLOR::RED);
}

template<typename T>
inline RBNode<T>*
RBTree<T>::root()
{
    return m_pRoot;
}

template<typename T>
inline bool
RBTree<T>::empty()
{
    return m_pRoot;
}

template<typename T>
inline void
_RBRotateLeft(RBTree<T>* s, RBNode<T>* elm)
{
    auto tmp = elm->right();
    if ((elm->right() = tmp->left()))
    {
        tmp->left()->setParent(elm);
    }

    tmp->setParent(elm->parentAndColor());
    if (tmp->parent())
    {
        if (elm == elm->parent()->left())
            elm->parent()->left() = tmp;
        else
            elm->parent()->right() = tmp;
    }
    else
        s->m_pRoot = tmp;

    tmp->left() = elm;
    elm->setParent(tmp);
}

template<typename T>
inline void
_RBRotateRight(RBTree<T>* s, RBNode<T>* elm)
{
    auto tmp = elm->left();
    if ((elm->left() = tmp->right()))
    {
        tmp->right()->setParent(elm);
    }

    tmp->setParent(elm->parentAndColor());
    if (tmp->parent())
    {
        if (elm == elm->parent()->left())
            elm->parent()->left() = tmp;
        else
            elm->parent()->right() = tmp;
    }
    else
        s->m_pRoot = tmp;

    tmp->right() = elm;
    elm->setParent(tmp);
}

template<typename T>
inline void
_RBInsertColor(RBTree<T>* s, RBNode<T>* elm)
{
    RBNode<T>* parent, * gparent, * tmp;
    while ((parent = elm->parent()) && parent->color() == RB_COLOR::RED)
    {
        gparent = parent->parent();
        if (parent == gparent->left())
        {
            tmp = gparent->right();
            if (tmp && tmp->color() == RB_COLOR::RED)
            {
                tmp->setColor(RB_COLOR::BLACK);
                _RBSetBlackRed(parent, gparent);
                elm = gparent;
                continue;
            }
            if (parent->right() == elm)
            {
                _RBRotateLeft(s, parent);
                tmp = parent;
                parent = elm;
                elm = tmp;
            }
            _RBSetBlackRed(parent, gparent);
            _RBRotateRight(s, gparent);
        }
        else
        {
            tmp = gparent->left();
            if (tmp && tmp->color() == RB_COLOR::RED)
            {
                tmp->setColor(RB_COLOR::BLACK);
                _RBSetBlackRed(parent, gparent);
                elm = gparent;
                continue;
            }
            if (parent->left() == elm)
            {
                _RBRotateRight(s, parent);
                tmp = parent;
                parent = elm;
                elm = tmp;
            }
            _RBSetBlackRed(parent, gparent);
            _RBRotateLeft(s, gparent);
        }
    }
    s->m_pRoot->setColor(RB_COLOR::BLACK);
}

template<typename T>
inline void
_RBRemoveColor(RBTree<T>* s, RBNode<T>* parent, RBNode<T>* elm)
{
    RBNode<T>* tmp;
    while ((elm == nullptr || elm->color() == RB_COLOR::BLACK) && elm != s->m_pRoot)
    {
        if (parent->left() == elm)
        {
            tmp = parent->right();
            if (tmp->color() == RB_COLOR::RED)
            {
                _RBSetBlackRed(tmp, parent);
                _RBRotateLeft(s, parent);
                tmp = parent->right();
            }
            if ((tmp->left() == nullptr || tmp->left()->color() == RB_COLOR::BLACK) &&
                (tmp->right() == nullptr || tmp->right()->color() == RB_COLOR::BLACK))
            {
                tmp->setColor(RB_COLOR::RED);
                elm = parent;
                parent = elm->parent();
            }
            else
            {
                if (tmp->right() == nullptr || tmp->right()->color() == RB_COLOR::BLACK)
                {
                    RBNode<T>* oleft;
                    if ((oleft = tmp->left()))
                        oleft->setColor(RB_COLOR::BLACK);
                    tmp->setColor(RB_COLOR::RED);
                    _RBRotateRight(s, tmp);
                    tmp = parent->right();
                }
                tmp->setColor(parent->color());
                parent->setColor(RB_COLOR::BLACK);
                if (tmp->right())
                    tmp->right()->setColor(RB_COLOR::BLACK);
                _RBRotateLeft(s, parent);
                elm = s->m_pRoot;
                break;
            }
        }
        else
        {
            tmp = parent->left();
            if (tmp->color() == RB_COLOR::RED)
            {
                _RBSetBlackRed(tmp, parent);
                _RBRotateRight(s, parent);
                tmp = parent->left();
            }
            if ((tmp->left() == nullptr || tmp->left()->color() == RB_COLOR::BLACK) &&
                (tmp->right() == nullptr || tmp->right()->color() == RB_COLOR::BLACK))
            {
                tmp->setColor(RB_COLOR::RED);
                elm = parent;
                parent = elm->parent();
            }
            else
            {
                if (tmp->left() == nullptr || tmp->left()->color() == RB_COLOR::BLACK)
                {
                    RBNode<T>* oright;
                    if ((oright = tmp->right()))
                        oright->setColor(RB_COLOR::BLACK);
                    tmp->setColor(RB_COLOR::RED);
                    _RBRotateLeft(s, tmp);
                    tmp = parent->left();
                }
                tmp->setColor(parent->color());
                parent->setColor(RB_COLOR::BLACK);
                if (tmp->left())
                    tmp->left()->setColor(RB_COLOR::BLACK);
                _RBRotateRight(s, parent);
                elm = s->m_pRoot;
                break;
            }
        }
    }
    if (elm)
        elm->setColor(RB_COLOR::BLACK);
}

template<typename T>
inline RBNode<T>*
RBTree<T>::remove(RBNode<T>* elm)
{
    ADT_ASSERT(m_size > 0, "empty");

    RBNode<T>* child, * parent, * old = elm;
    enum RB_COLOR color;
    if (elm->left() == nullptr)
        child = elm->right();
    else if (elm->right() == nullptr)
        child = elm->left();
    else
    {
        RBNode<T>* left;
        elm = elm->right();
        while ((left = elm->left()))
            elm = left;
        child = elm->right();
        parent = elm->parent();
        color = elm->color();
        if (child)
            child->setParent(parent);
        if (parent)
        {
            if (parent->left() == elm)
                parent->left() = child;
            else
                parent->right() = child;
        }
        else
            m_pRoot = child;
        if (elm->parent() == old)
            parent = elm;

        _RBSetLinks(elm, old);

        if (old->parent())
        {
            if (old->parent()->left() == old)
                old->parent()->left() = elm;
            else
                old->parent()->right() = elm;
        }
        else
            m_pRoot = elm;
        old->left()->setParent(elm);
        if (old->right())
            old->right()->setParent(elm);
        goto GOTO_color;
    }
    parent = elm->parent();
    color = elm->color();
    if (child)
        child->setParent(parent);
    if (parent)
    {
        if (parent->left() == elm)
            parent->left() = child;
        else
            parent->right() = child;
    }
    else
        m_pRoot = child;
GOTO_color:
    if (color == RB_COLOR::BLACK)
        _RBRemoveColor(this, parent, child);

    --m_size;
    return (old);
}

template<typename T>
inline void
RBTree<T>::removeAndFree(IAllocator* p, RBNode<T>* elm)
{
    p->free(remove(elm));
}

template<typename T>
inline void
RBTree<T>::removeAndFree(IAllocator* p, const T& elm)
{
    p->free(RBSearch(m_pRoot, elm));
}

/* create RBNode outside then insert */
template<typename T>
inline RBNode<T>*
RBTree<T>::insert(bool bAllowDuplicates, RBNode<T>* elm)
{
    RBNode<T>* parent = nullptr;
    RBNode<T>* tmp = m_pRoot;
    isize comp = 0;
    while (tmp)
    {
        parent = tmp;
        comp = utils::compare(elm->m_data, parent->m_data);

        if (comp == 0)
        {
            /* left case */
            if (bAllowDuplicates) tmp = tmp->left();
            else return tmp;
        }
        else if (comp < 0) tmp = tmp->left();
        else tmp = tmp->right();
    }

    _RBSet(elm, parent);

    if (parent != nullptr)
    {
        if (comp <= 0) parent->left() = elm;
        else parent->right() = elm;
    }
    else m_pRoot = elm;

    _RBInsertColor(this, elm);
    ++m_size;
    return elm;
}

template<typename T>
inline RBNode<T>*
RBTree<T>::insert(IAllocator* pA, bool bAllowDuplicates, const T& data)
{
    RBNode<T>* pNew = RBNodeAlloc(pA, data);
    return insert(bAllowDuplicates, pNew);
}

template<typename T>
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>) 
inline RBNode<T>*
RBTree<T>::emplace(IAllocator* pA, bool bAllowDuplicates, ARGS&&... args)
{
    RBNode<T>* pNew = RBNodeAlloc(pA, std::forward<ARGS>(args)...);
    return insert(bAllowDuplicates, pNew);
}

template<typename T>
inline RBNode<T>*
RBNodeAlloc(IAllocator* pA, const T& data)
{
    auto* r = (RBNode<T>*)pA->zalloc(1, sizeof(RBNode<T>));
    new(&r->m_data) T(data);
    return r;
}

template<typename T, typename ...ARGS>
inline RBNode<T>*
RBNodeAlloc(IAllocator* pA, ARGS&& ...args)
{
    auto* r = (RBNode<T>*)pA->zalloc(1, sizeof(RBNode<T>));
    new(&r->m_data) T(std::forward<ARGS>(args)...);
    return r;
}

template<typename T>
inline RBNode<T>*
RBTraversePRE(
    RBNode<T>* p,
    bool (*pfn)(RBNode<T>* pNode, void* pArg),
    void* pUserData
)
{
    if (p)
    {
        if (pfn(p, pUserData)) return {p};
        RBTraversePRE(p->left(), pfn, pUserData);
        RBTraversePRE(p->right(), pfn, pUserData);
    }

    return {};
}

template<typename T>
inline RBNode<T>*
RBTraverseIN(
    RBNode<T>* p,
    bool (*pfn)(RBNode<T>* pNode, void* pArg),
    void* pUserData
)
{
    if (p)
    {
        RBTraverseIN(p->left(), pfn, pUserData);
        if (pfn(p, pUserData)) return {p};
        RBTraverseIN(p->right(), pfn, pUserData);
    }

    return {};
}

template<typename T>
inline RBNode<T>*
RBTraversePOST(
    RBNode<T>* p,
    bool (*pfn)(RBNode<T>* pNode, void* pArg),
    void* pUserData
)
{
    if (p)
    {
        RBTraversePOST(p->left(), pfn, pUserData);
        RBTraversePOST(p->right(), pfn, pUserData);
        if (pfn(p, pUserData)) return {p};
    }

    return {};
}

/* early return if pfn returns true */
template<typename T>
inline RBNode<T>*
RBTraverse(
    RBNode<T>* p,
    bool (*pfn)(RBNode<T>* pNode, void* pArg),
    void* pUserData,
    RB_ORDER order
)
{
    switch (order)
    {
        case RB_ORDER::PRE:
        return RBTraversePRE(p, pfn, pUserData);

        case RB_ORDER::IN:
        return RBTraverseIN(p, pfn, pUserData);

        case RB_ORDER::POST:
        return RBTraversePOST(p, pfn, pUserData);
    }

    ADT_ASSERT(false, "incorrect RB_ORDER");
    return {};
}

template<typename T>
inline RBNode<T>*
RBSearch(RBNode<T>* p, const T& data)
{
    auto it = p;
    while (it)
    {
        isize cmp = utils::compare(data, it->m_data);
        if (cmp == 0) return it;
        else if (cmp < 0) it = it->left();
        else it = it->right();
    }

    return nullptr;
}

template<typename T>
inline int
RBDepth(RBNode<T>* p)
{
    if (p)
    {
        int l = RBDepth(p->left());
        int r = RBDepth(p->right());
        return 1 + utils::max(l, r);
    } else return 0;
}

template<typename T>
inline void
RBPrintNodes(
    IAllocator* pA,
    const RBNode<T>* pNode,
    FILE* pF,
    const StringView sPrefix,
    bool bLeft
)
{
    if (pNode)
    {
        print::toFILE(pF, "{}{} {}\n", sPrefix, bLeft ? "|__" : "\\__", *pNode);

        String sCat = StringCat(pA, sPrefix, bLeft ? "|   " : "    ");

        RBPrintNodes(pA, pNode->left(), pF, sCat, true);
        RBPrintNodes(pA, pNode->right(), pF, sCat, false);

        pA->free(sCat.m_pData);
    }
}

template<typename T>
inline void
RBTree<T>::destroy(IAllocator* pAlloc)
{
    auto pfnFree = +[](RBNode<T>* p, void* ptr) -> bool {
        ((IAllocator*)ptr)->free(p);

        return false;
    };

    RBTraverse(m_pRoot, pfnFree, pAlloc, RB_ORDER::POST);
}

namespace print
{

template<typename T>
inline isize
formatToContext(Context ctx, [[maybe_unused]]  FormatArgs fmtArgs, const RBNode<T>& node)
{
    char aBuff[128] {};
    const StringView sCol = node.color() == RB_COLOR::BLACK ? ADT_LOGS_COL_BLUE : ADT_LOGS_COL_RED;
    print::toBuffer(aBuff, utils::size(aBuff), "{}{}" ADT_LOGS_COL_NORM, sCol, node.m_data);

    return copyBackToContext(ctx, fmtArgs, {aBuff});
}

} /* namespace print */

} /* namespace adt */

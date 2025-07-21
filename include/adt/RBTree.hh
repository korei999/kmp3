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
#include "logs.hh"
#include "utils.hh"

#include <cstdio>

#ifdef _WIN32
    #undef IN
#endif

namespace adt
{

enum class RB_COLOR : u8 { BLACK, RED };
enum class RB_ORDER : u8 { PRE, IN, POST };

template<typename T>
struct RBTree
{
    struct Node
    {
        static constexpr usize COLOR_MASK = 1ULL;

        /* */

        Node* m_left {};
        Node* m_right {};
        Node* m_parentColor {}; /* NOTE: color is the least significant bit */
        ADT_NO_UNIQUE_ADDRESS T m_data {};

        /* */

        T& data() { return m_data; }
        const T& data() const { return m_data; }

        Node*& left() { return m_left; }
        Node* const& left() const { return m_left; }
        Node*& right() { return m_right; }
        Node* const& right() const { return m_right; }

        RB_COLOR color() const { return (RB_COLOR)((usize)m_parentColor & COLOR_MASK); }
        RB_COLOR setColor(const RB_COLOR eColor) { m_parentColor = (Node*)((usize)parent() | (usize)eColor); return eColor; }

        Node* parent() const { return (Node*)((usize)m_parentColor & ~COLOR_MASK); }
        void setParent(Node* par) { m_parentColor = (Node*)(((usize)par & ~COLOR_MASK) | (usize)color()); }

        Node*& parentAndColor() { return m_parentColor; }
        Node* const& parentAndColor() const { return m_parentColor; }
    };

    /* */

    Node* m_pRoot = nullptr;
    usize m_size = 0;

    /* */

    Node* root() noexcept { return m_pRoot; }
    const Node* root() const noexcept { return m_pRoot; }
    const Node* data() const noexcept { return m_pRoot; };
    Node* data() noexcept { return m_pRoot; };

    bool empty();
    Node* remove(Node* elm);

    void removeAndFree(IAllocator* p, Node* elm);
    void removeAndFree(IAllocator* p, const T& elm);

    Node* insertNode(bool bAllowDuplicates, Node* elm);

    Node* insert(IAllocator* pA, bool bAllowDuplicates, const T& data);
    Node* insert(IAllocator* pA, bool bAllowDuplicates, T&& data);

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    Node* emplace(IAllocator* pA, bool bAllowDuplicates, ARGS&&... args);

    RBTree release() noexcept { return utils::exchange(this, {}); }

    void destroy(IAllocator* pA) noexcept;

    void destructElements() noexcept;

    Node* leftMost() noexcept;
    static Node* leftMost(Node* p) noexcept;
    static Node* successor(Node* p) noexcept;

    Node* rightMost() noexcept { return rightMost(m_pRoot); }
    static Node* rightMost(Node* p) noexcept;
    static Node* predecessor(Node* p) noexcept;

    template<typename ...ARGS>
    [[nodiscard]] static inline Node* allocNode(IAllocator* pA, ARGS&&... args);

    template<typename LAMBDA>
    static inline Node* traversePre(Node* p, LAMBDA cl);

    template<typename LAMBDA>
    static inline void traversePreNoReturn(Node* p, LAMBDA cl);

    template<typename LAMBDA>
    static inline Node* traverseIn(Node* p, LAMBDA cl);

    template<typename LAMBDA>
    static inline void traverseInNoReturn(Node* p, LAMBDA cl);

    template<typename LAMBDA>
    static inline Node* traversePost(Node* p, LAMBDA cl);

    template<typename LAMBDA>
    static inline void traversePostNoReturn(Node* p, LAMBDA cl);

    template<typename LAMBDA>
    static inline Node* traverse(Node* p, LAMBDA cl, RB_ORDER order);

    static inline Node* search(Node* p, const T& data);

    static inline int depth(Node* p);

    static inline void printNodes(
        IAllocator* pA,
        const Node* pNode,
        FILE* pF,
        const StringView sPrefix = "",
        bool bLeft = false
    );

    struct It
    {
        Node* m_pCurrent {};

        It() = default;
        It(const Node* pHead) : m_pCurrent {const_cast<Node*>(pHead)} {}

        T& operator*() noexcept { return m_pCurrent->m_data; }
        T* operator->() noexcept { return &m_pCurrent->m_data; }

        It operator++() noexcept { return m_pCurrent = successor(m_pCurrent); }
        It operator--() noexcept { return m_pCurrent = predecessor(m_pCurrent); }

        It current() noexcept { return m_pCurrent; }
        const It current() const noexcept { return m_pCurrent; }

        It next() noexcept { return successor(m_pCurrent); }
        const It next() const noexcept { return successor(m_pCurrent); }

        friend constexpr bool operator==(const It& l, const It& r) noexcept { return l.m_pCurrent == r.m_pCurrent; }
        friend constexpr bool operator!=(const It& l, const It& r) noexcept { return l.m_pCurrent != r.m_pCurrent; }
    };

    It begin() noexcept { return leftMost(m_pRoot); }
    It end() noexcept { return nullptr; }
    It rbegin() noexcept { return rightMost(m_pRoot); }
    It rend() noexcept { return nullptr; }

    const It begin() const noexcept { return leftMost(m_pRoot); }
    const It end() const noexcept { return nullptr; }
    const It rbegin() const noexcept { return rightMost(m_pRoot); }
    const It rend() const noexcept { return nullptr; }

protected:
    static Node* nextGreaterParent(Node* p) noexcept;
    static inline void setBlackRed(Node* black, Node* red);
    static inline void set(Node* elm, Node* parent);
    static inline void setLinks(Node* l, Node* r);
    static inline void rotateLeft(RBTree<T>* s, Node* elm);
    static inline void rotateRight(RBTree<T>* s, Node* elm);
    static inline void insertColor(RBTree<T>* s, Node* elm);
    static inline void removeColor(RBTree<T>* s, Node* parent, Node* elm);
};

template<typename T>
inline void
RBTree<T>::setLinks(Node* l, Node* r)
{
    l->left() = r->left();
    l->right() = r->right();
    l->parentAndColor() = r->parentAndColor();
}

template<typename T>
inline void
RBTree<T>::set(Node* elm, Node* parent)
{
    elm->setParent(parent);
    elm->left() = elm->right() = nullptr;
    elm->setColor(RB_COLOR::RED);
}

template<typename T>
inline void
RBTree<T>::setBlackRed(Node* black, Node* red)
{
    black->setColor(RB_COLOR::BLACK);
    red->setColor(RB_COLOR::RED);
}

template<typename T>
inline bool
RBTree<T>::empty()
{
    return m_pRoot;
}

template<typename T>
inline void
RBTree<T>::rotateLeft(RBTree<T>* s, Node* elm)
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
RBTree<T>::rotateRight(RBTree<T>* s, Node* elm)
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
RBTree<T>::insertColor(RBTree<T>* s, Node* elm)
{
    Node* parent, * gparent, * tmp;
    while ((parent = elm->parent()) && parent->color() == RB_COLOR::RED)
    {
        gparent = parent->parent();
        if (parent == gparent->left())
        {
            tmp = gparent->right();
            if (tmp && tmp->color() == RB_COLOR::RED)
            {
                tmp->setColor(RB_COLOR::BLACK);
                setBlackRed(parent, gparent);
                elm = gparent;
                continue;
            }
            if (parent->right() == elm)
            {
                rotateLeft(s, parent);
                tmp = parent;
                parent = elm;
                elm = tmp;
            }
            setBlackRed(parent, gparent);
            rotateRight(s, gparent);
        }
        else
        {
            tmp = gparent->left();
            if (tmp && tmp->color() == RB_COLOR::RED)
            {
                tmp->setColor(RB_COLOR::BLACK);
                setBlackRed(parent, gparent);
                elm = gparent;
                continue;
            }
            if (parent->left() == elm)
            {
                rotateRight(s, parent);
                tmp = parent;
                parent = elm;
                elm = tmp;
            }
            setBlackRed(parent, gparent);
            rotateLeft(s, gparent);
        }
    }
    s->m_pRoot->setColor(RB_COLOR::BLACK);
}

template<typename T>
inline void
RBTree<T>::removeColor(RBTree<T>* s, Node* parent, Node* elm)
{
    Node* tmp;
    while ((elm == nullptr || elm->color() == RB_COLOR::BLACK) && elm != s->m_pRoot)
    {
        if (parent->left() == elm)
        {
            tmp = parent->right();
            if (tmp->color() == RB_COLOR::RED)
            {
                setBlackRed(tmp, parent);
                rotateLeft(s, parent);
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
                    Node* oleft;
                    if ((oleft = tmp->left()))
                        oleft->setColor(RB_COLOR::BLACK);
                    tmp->setColor(RB_COLOR::RED);
                    rotateRight(s, tmp);
                    tmp = parent->right();
                }
                tmp->setColor(parent->color());
                parent->setColor(RB_COLOR::BLACK);
                if (tmp->right())
                    tmp->right()->setColor(RB_COLOR::BLACK);
                rotateLeft(s, parent);
                elm = s->m_pRoot;
                break;
            }
        }
        else
        {
            tmp = parent->left();
            if (tmp->color() == RB_COLOR::RED)
            {
                setBlackRed(tmp, parent);
                rotateRight(s, parent);
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
                    Node* oright;
                    if ((oright = tmp->right()))
                        oright->setColor(RB_COLOR::BLACK);
                    tmp->setColor(RB_COLOR::RED);
                    rotateLeft(s, tmp);
                    tmp = parent->left();
                }
                tmp->setColor(parent->color());
                parent->setColor(RB_COLOR::BLACK);
                if (tmp->left())
                    tmp->left()->setColor(RB_COLOR::BLACK);
                rotateRight(s, parent);
                elm = s->m_pRoot;
                break;
            }
        }
    }
    if (elm)
        elm->setColor(RB_COLOR::BLACK);
}

template<typename T>
inline RBTree<T>::Node*
RBTree<T>::remove(Node* elm)
{
    ADT_ASSERT(m_size > 0, "empty");

    Node* child, * parent, * old = elm;
    enum RB_COLOR color;
    if (elm->left() == nullptr)
        child = elm->right();
    else if (elm->right() == nullptr)
        child = elm->left();
    else
    {
        Node* left;
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

        setLinks(elm, old);

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
        removeColor(this, parent, child);

    --m_size;
    return (old);
}

template<typename T>
inline void
RBTree<T>::removeAndFree(IAllocator* p, Node* elm)
{
    p->free(remove(elm));
}

template<typename T>
inline void
RBTree<T>::removeAndFree(IAllocator* p, const T& elm)
{
    removeAndFree(p, search(m_pRoot, elm));
}

/* create RBNode outside then insert */
template<typename T>
inline RBTree<T>::Node*
RBTree<T>::insertNode(bool bAllowDuplicates, Node* elm)
{
    Node* parent = nullptr;
    Node* tmp = m_pRoot;
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

    set(elm, parent);

    if (parent != nullptr)
    {
        if (comp <= 0) parent->left() = elm;
        else parent->right() = elm;
    }
    else m_pRoot = elm;

    insertColor(this, elm);
    ++m_size;
    return elm;
}

template<typename T>
inline RBTree<T>::Node*
RBTree<T>::insert(IAllocator* pA, bool bAllowDuplicates, const T& data)
{
    Node* pNew = allocNode<T>(pA, data);
    return insertNode(bAllowDuplicates, pNew);
}

template<typename T>
inline RBTree<T>::Node*
RBTree<T>::insert(IAllocator* pA, bool bAllowDuplicates, T&& data)
{
    Node* pNew = allocNode<T>(pA, std::move(data));
    return insertNode(bAllowDuplicates, pNew);
}

template<typename T>
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline RBTree<T>::Node*
RBTree<T>::emplace(IAllocator* pA, bool bAllowDuplicates, ARGS&&... args)
{
    Node* pNew = allocNode<T>(pA, std::forward<ARGS>(args)...);
    return insertNode(bAllowDuplicates, pNew);
}

template<typename T>
template<typename ...ARGS>
inline RBTree<T>::Node*
RBTree<T>::allocNode(IAllocator* pA, ARGS&& ...args)
{
    static_assert(std::is_constructible_v<T, ARGS...>);

    auto* r = pA->zallocV<Node>(1);
    new(&r->m_data) T (std::forward<ARGS>(args)...);
    return r;
}

template<typename T>
template<typename LAMBDA>
inline RBTree<T>::Node*
RBTree<T>::traversePre(Node* p, LAMBDA cl)
{
    for (; p; p = p->right())
    {
        if (cl(p)) return {p};
        if (auto* pFound = traversePre(p->left(), cl)) return pFound;
    }

    return {};
}

template<typename T>
template<typename LAMBDA>
inline void
RBTree<T>::traversePreNoReturn(Node* p, LAMBDA cl)
{
    for (; p; p = p->right())
    {
        cl(p);
        traversePreNoReturn(p->left(), cl);
    }
}

template<typename T>
template<typename LAMBDA>
inline RBTree<T>::Node*
RBTree<T>::traverseIn(Node* p, LAMBDA cl)
{
    if (p)
    {
        if (auto* pFound = traverseIn(p->left(), cl)) return pFound;
        if (cl(p)) return {p};
        if (auto* pFound = traverseIn(p->right(), cl)) return pFound;
    }

    return {};
}

template<typename T>
template<typename LAMBDA>
inline void
RBTree<T>::traverseInNoReturn(Node* p, LAMBDA cl)
{
    if (p)
    {
        traverseInNoReturn(p->left(), cl);
        cl(p);
        traverseInNoReturn(p->right(), cl);
    }
}

template<typename T>
template<typename LAMBDA>
inline RBTree<T>::Node*
RBTree<T>::traversePost(Node* p, LAMBDA cl)
{
    if (p)
    {
        if (auto* pFound = traversePost(p->left(), cl)) return pFound;
        if (auto* pFound = traversePost(p->right(), cl)) return pFound;
        if (cl(p)) return {p};
    }

    return {};
}

template<typename T>
template<typename LAMBDA>
inline void
RBTree<T>::traversePostNoReturn(Node* p, LAMBDA cl)
{
    if (p)
    {
        traversePostNoReturn(p->left(), cl);
        traversePostNoReturn(p->right(), cl);
        cl(p);
    }
}

/* early return if pfn returns true */
template<typename T>
template<typename LAMBDA>
inline RBTree<T>::Node*
RBTree<T>::traverse(Node* p, LAMBDA cl, RB_ORDER order)
{
    switch (order)
    {
        case RB_ORDER::PRE:
        return traversePre(p, cl);

        case RB_ORDER::IN:
        return traverseIn(p, cl);

        case RB_ORDER::POST:
        return traversePost(p, cl);
    }

    ADT_ASSERT(false, "incorrect RB_ORDER");
    return {};
}

template<typename T>
inline RBTree<T>::Node*
RBTree<T>::search(Node* p, const T& data)
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
RBTree<T>::depth(Node* p)
{
    if (p)
    {
        int l = depth(p->left());
        int r = depth(p->right());
        return 1 + utils::max(l, r);
    } else return 0;
}

template<typename T>
inline void
RBTree<T>::printNodes(
    IAllocator* pA,
    const Node* pNode,
    FILE* pF,
    const StringView sPrefix,
    bool bLeft
)
{
    if (pNode)
    {
        const StringView sCol = pNode->color() == RB_COLOR::BLACK ? ADT_LOGS_COL_BLUE : ADT_LOGS_COL_RED;
        print::toFILE(pF, "{}{} {}{}" ADT_LOGS_COL_NORM "\n", sPrefix, bLeft ? "|__" : "\\__", sCol, pNode->m_data);

        String sCat = StringCat(pA, sPrefix, bLeft ? "|   " : "    ");

        printNodes(pA, pNode->left(), pF, sCat, true);
        printNodes(pA, pNode->right(), pF, sCat, false);

        pA->free(sCat.m_pData);
    }
}

template<typename T>
inline void
RBTree<T>::destroy(IAllocator* pAlloc) noexcept
{
    traversePost(m_pRoot, [&](Node* p)
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
                p->m_data.~T();
            pAlloc->free(p);
            return false;
        }
    );
    *this = {};
}

template<typename T>
inline void
RBTree<T>::destructElements() noexcept
{
    traversePre(m_pRoot, [&](Node* p)
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
                p->m_data.~T();
            return false;
        }
    );
}

template<typename T>
inline RBTree<T>::Node*
RBTree<T>::leftMost() noexcept
{
    return leftMost(m_pRoot);
}

template<typename T>
inline RBTree<T>::Node*
RBTree<T>::leftMost(Node* p) noexcept
{
    Node* pPrev = nullptr;
    while (p)
    {
        pPrev = p;
        p = p->left();
    }

    return pPrev;
}

template<typename T>
inline RBTree<T>::Node*
RBTree<T>::successor(Node* p) noexcept
{
    if (!p) return nullptr;

    if (auto* pRight = p->right())
        return leftMost(pRight);

    Node* pParent;
    while ((pParent = p->parent()) && p == pParent->right())
        p = p->parent();

    return p->parent();
}

template<typename T>
inline RBTree<T>::Node*
RBTree<T>::rightMost(Node* p) noexcept
{
    Node* pPrev = nullptr;
    while (p)
    {
        pPrev = p;
        p = p->right();
    }

    return pPrev;
}

template<typename T>
inline RBTree<T>::Node*
RBTree<T>::predecessor(Node* p) noexcept
{
    if (!p) return nullptr;

    if (auto* pRight = p->left())
        return rightMost(pRight);

    Node* pParent;
    while ((pParent = p->parent()) && p == pParent->left())
        p = p->parent();

    return p->parent();
}

template<typename T>
inline RBTree<T>::Node*
RBTree<T>::nextGreaterParent(Node* p) noexcept
{
    const Node* pUs = p;
    while (p)
    {
        auto* pPrev = p;
        p = p->parent();
        if (p && p->left() == pPrev) return p;
    }

    return nullptr;
}

} /* namespace adt */

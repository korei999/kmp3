/* Borrowed from OpenBSD's splay tree implementation. */

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
#include "defer.hh"
#include "print.inc"

namespace adt
{

template<typename T>
struct SplayTree;

template<typename T>
struct SplayTreeNode
{
    using Node = SplayTreeNode<T>;

    /* */

    Node* m_pLeft {};
    Node* m_pRight {};
    ADT_NO_UNIQUE_ADDRESS T m_data {};

    /* */

    template<typename ...ARGS>
    static Node*
    alloc(IAllocator* pAlloc, ARGS&&... args)
    {
        Node* p = pAlloc->mallocV<Node>(1);
        new(&p->m_data) T (std::forward<ARGS>(args)...);
        p->m_pLeft = p->m_pRight = nullptr;
        return p;
    };

    /* */

    T& data() noexcept { return m_data; }
    const T& data() const noexcept { return m_data; }

    void print(IAllocator* pAlloc, FILE* pF, const StringView svPrefix = "");
};

template<typename T>
struct SplayTree
{
    using Node = SplayTreeNode<T>;
    static_assert(std::is_same_v<Node, typename SplayTreeNode<T>::Node>);

    /* */

    Node* m_pRoot;

    /* */

    SplayTree() : m_pRoot {nullptr} {}

    /* */

    static inline void print(
        IAllocator* pAlloc,
        FILE* pF,
        const Node* pNode,
        const StringView svPrefix = "",
        bool bLeft = false,
        bool bHasRightSibling = false /* Fix for trailing prefixes. */
    );

    /* */

    void print(
        IAllocator* pAlloc,
        FILE* pF,
        const StringView svPrefix = ""
    ) { print(pAlloc, pF, m_pRoot, svPrefix); }

    Node* root() noexcept { return m_pRoot; }

    template<typename ...ARGS>
    Node* insert(IAllocator* pAlloc, ARGS&&... args);
    Node* insertNode(Node* p) noexcept;
    Node* search(const T& key) noexcept;
    Node* remove(Node* p) noexcept;

    /* */

protected:
    void splay(const T& key) noexcept;

    static void rotateRight(SplayTree* head, Node* p) noexcept;
    static void rotateLeft(SplayTree* head, Node* p) noexcept;
    static void linkLeft(SplayTree* head, Node** p) noexcept;
    static void linkRight(SplayTree* head, Node** p) noexcept;
    static void assemble(SplayTree* head, Node* node, Node* left, Node* right) noexcept;
};

template<typename T>
inline void
SplayTreeNode<T>::print(IAllocator* pAlloc, FILE* pF, const StringView svPrefix)
{
    SplayTree<T>::print(pAlloc, pF, this, svPrefix);
}

template<typename T>
inline SplayTree<T>::Node*
SplayTree<T>::insertNode(Node* p) noexcept
{
    ADT_ASSERT(p != nullptr, "");

    if (!m_pRoot)
    {
        p->m_pLeft = p->m_pRight = nullptr;
    }
    else
    {
        isize cmp;
        splay(p->m_data);
        cmp = utils::compare(p->m_data, m_pRoot->m_data);
        if (cmp < 0)
        {
            p->m_pLeft = m_pRoot->m_pLeft;
            p->m_pRight = m_pRoot;
            m_pRoot->m_pLeft = nullptr;
        }
        else if (cmp > 0)
        {
            p->m_pRight = m_pRoot->m_pRight;
            p->m_pLeft = m_pRoot;
            m_pRoot->m_pRight = nullptr;
        }
        else
        {
            return m_pRoot;
        }
    }
    m_pRoot = p;
    return m_pRoot;
}

template<typename T>
inline SplayTree<T>::Node*
SplayTree<T>::search(const T& key) noexcept
{
    if (!m_pRoot) return nullptr;

    splay(key);
    if (utils::compare(key, m_pRoot->m_data) == 0) return m_pRoot;
    else return nullptr;
}

template<typename T>
inline SplayTree<T>::Node*
SplayTree<T>::remove(Node* p) noexcept
{
    Node* tmp;
    if (!m_pRoot) return nullptr;

    splay(p->m_data);
    if (utils::compare(p->m_data, m_pRoot->m_data) == 0)
    {
        if (!m_pRoot->m_pLeft)
        {
            m_pRoot = m_pRoot->m_pRight;
        }
        else
        {
            tmp = m_pRoot->m_pRight;
            m_pRoot = m_pRoot->m_pLeft;
            splay(p->m_data);
            m_pRoot->m_pRight = tmp;
        }
        return p;
    }
    return nullptr;
}

template<typename T>
template<typename ...ARGS>
inline SplayTree<T>::Node*
SplayTree<T>::insert(IAllocator* pAlloc, ARGS&&... args)
{
    return insertNode(Node::alloc(pAlloc, std::forward<ARGS>(args)...));
}

template<typename T>
inline void
SplayTree<T>::splay(const T& key) noexcept
{
    Node node {}, * left {}, * right {}, * tmp {};
    isize cmp;

    left = right = &node;

    while ((cmp = utils::compare(key, m_pRoot->m_data)))
    {
        if (cmp < 0)
        {
            tmp = m_pRoot->m_pLeft;
            if (!tmp) break;

            if (utils::compare(key, tmp->m_data) < 0)
            {
                rotateRight(this, tmp);
                if (!m_pRoot->m_pLeft) break;
            }
            linkLeft(this, &right);
        }
        else if (cmp > 0)
        {
            tmp = m_pRoot->m_pRight;
            if (!tmp) break;

            if (utils::compare(key, tmp->m_data) > 0)
            {
                rotateLeft(this, tmp);
                if (!m_pRoot->m_pRight) break;
            }
            linkRight(this, &left);
        }
    }
    assemble(this, &node, left, right);
}

template<typename T>
inline void
SplayTree<T>::rotateRight(SplayTree* head, Node* p) noexcept
{
    head->m_pRoot->m_pLeft = p->m_pRight;
    p->m_pRight = head->m_pRoot;
    head->m_pRoot = p;
}

template<typename T>
inline void
SplayTree<T>::rotateLeft(SplayTree* head, Node* p) noexcept
{
    head->m_pRoot->m_pRight = p->m_pLeft;
    p->m_pLeft = head->m_pRoot;
    head->m_pRoot = p;
}

template<typename T>
inline void
SplayTree<T>::linkLeft(SplayTree* head, Node** p) noexcept
{
    (*p)->m_pLeft = head->m_pRoot;
    *p = head->m_pRoot;
    head->m_pRoot = head->m_pRoot->m_pLeft;
}

template<typename T>
inline void
SplayTree<T>::linkRight(SplayTree* head, Node** p) noexcept
{
    (*p)->m_pRight = head->m_pRoot;
    *p = head->m_pRoot;
    head->m_pRoot = head->m_pRoot->m_pRight;
}

template<typename T>
inline void
SplayTree<T>::assemble(SplayTree* head, Node* node, Node* left, Node* right) noexcept
{
    left->m_pRight = head->m_pRoot->m_pLeft;
    right->m_pLeft = head->m_pRoot->m_pRight;
    head->m_pRoot->m_pLeft = node->m_pRight;
    head->m_pRoot->m_pRight = node->m_pLeft;
}

template<typename T>
inline void
SplayTree<T>::print(
    IAllocator* pAlloc,
    FILE* pF,
    const Node* pNode,
    const StringView svPrefix,
    bool bLeft,
    bool bHasRightSibling
)
{
    if (pNode)
    {
        print::toFILE(pAlloc, pF, "{}{} {}\n", svPrefix, bLeft ? "├──" : "└──", pNode->m_data);

        String sCat = StringCat(pAlloc, svPrefix, bHasRightSibling && bLeft ? "│   " : "    ");
        ADT_DEFER( pAlloc->free(sCat.m_pData) );

        print(pAlloc, pF, pNode->m_pLeft, sCat, true, pNode->m_pRight);
        print(pAlloc, pF, pNode->m_pRight, sCat, false, false);
    }
}

namespace print
{

template<typename T>
inline isize
format(Context ctx, FormatArgs fmtArgs, const SplayTreeNode<T>* const x)
{
    if (x) return format(ctx, fmtArgs, x->m_data);
    else return format(ctx, fmtArgs, nullptr);
}

} /* namespace format */

} /* namespace adt */

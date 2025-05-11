#pragma once

#include "adt/IAllocator.hh"

namespace adt
{

template<typename  T>
struct SList
{
    struct Node
    {
        ADT_NO_UNIQUE_ADDRESS Node* pNext {};
        ADT_NO_UNIQUE_ADDRESS T data {};

        /* */

        [[nodiscard("leak")]] static Node* alloc(IAllocator* pAlloc, const T& x);
    };

    /* */

    Node* m_pHead {};

    /* */

    Node* insert(IAllocator* pAlloc, const T& x); /* prepend */

    void remove(Node* pNode); /* O(n) */

    void removeFree(IAllocator* pAlloc, Node* pNode);

    void remove(Node* pPrev, Node* pNode);

    /* */

    struct It
    {
        Node* m_prev {};
        Node* m_current {};

        It() = default;
        It(const Node* pHead) : m_current {const_cast<Node*>(pHead)} {}

        T& operator*() noexcept { return m_current->data; }
        T* operator->() noexcept { return &m_current->data; }

        It operator++() noexcept { m_prev = m_current; return m_current = m_current->pNext; }

        Node* current() noexcept { return m_current; }
        const Node* current() const noexcept { return m_current; }

        Node* prev() noexcept { return m_prev; }
        const Node* prev() const noexcept { return m_prev; }

        friend constexpr bool operator==(const It& l, const It& r) noexcept { return l.m_current == r.m_current; }
        friend constexpr bool operator!=(const It& l, const It& r) noexcept { return l.m_current != r.m_current; }
    };

    It begin() noexcept { return {m_pHead}; }
    It end() noexcept { return {}; }

    const It begin() const noexcept { return {m_pHead}; }
    const It end() const  noexcept { return {}; }
};

template<typename T>
inline SList<T>::Node*
SList<T>::Node::alloc(IAllocator* pAlloc, const T& x)
{
    Node* pNew = static_cast<Node*>(pAlloc->zalloc(1, sizeof(Node)));
    new(&pNew->data) T {x};
    return pNew;
}

template<typename T>
inline SList<T>::Node*
SList<T>::insert(IAllocator* pAlloc, const T& x)
{
    Node* pNew = Node::alloc(pAlloc, x);

    pNew->pNext = m_pHead;
    m_pHead = pNew;

    return pNew;
}

template<typename T>
inline void
SList<T>::remove(Node* pNode)
{
    for (auto it = begin(); it != end(); ++it)
    {
        if (it.current() == pNode)
        {
            if (it.prev()) it.prev()->pNext = it.current()->pNext;
            else m_pHead = it.current()->pNext;

            break;
        }
    }
}

template<typename T>
inline void
SList<T>::removeFree(IAllocator* pAlloc, Node* pNode)
{
    remove(pNode);
    pAlloc->free(pNode);
}

template<typename T>
inline void
SList<T>::remove(Node* pPrev, Node* pNode)
{
    if (pNode == m_pHead)
    {
        m_pHead = pNode->pNext;
    }
    else
    {
        ADT_ASSERT(pPrev != nullptr, "");
        pPrev->pNext = pNode->pNext;
    }
}

} /* namespace adt */

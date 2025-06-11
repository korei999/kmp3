#pragma once

#include "StdAllocator.hh"

namespace adt
{

template<typename T>
struct SList
{
    struct Node
    {
        Node* pNext {};
        ADT_NO_UNIQUE_ADDRESS T data {};

        /* */

        [[nodiscard("leak")]] static Node* alloc(IAllocator* pAlloc, const T& x);
    };

    /* */

    Node* m_pHead {};

    /* */

    Node* data() noexcept { return m_pHead; }
    const Node* data() const noexcept { return m_pHead; }

    Node* insert(IAllocator* pAlloc, const T& x); /* prepend */
    Node* insert(Node* pNode); /* prepend */

    void remove(Node* pNode); /* O(n) */
    void remove(IAllocator* pAlloc, Node* pNode); /* O(n) */
    void remove(Node* pPrev, Node* pNode); /* O(1) */
    void remove(IAllocator* pAlloc, Node* pPrev, Node* pNode); /* O(1); free(pNode) */

    void destroy(IAllocator* pAlloc) noexcept;

    SList release() noexcept;

    /* */

    struct It
    {
        Node* m_current {};

        It() = default;
        It(const Node* pHead) : m_current {const_cast<Node*>(pHead)} {}

        T& operator*() noexcept { return m_current->data; }
        T* operator->() noexcept { return &m_current->data; }

        It operator++() noexcept { return m_current = m_current->pNext; }

        Node* current() noexcept { return m_current; }
        const Node* current() const noexcept { return m_current; }

        Node* next() noexcept { return m_current->pNext; }
        const Node* next() const noexcept { return m_current->pNext; }

        friend constexpr bool operator==(const It& l, const It& r) noexcept { return l.m_current == r.m_current; }
        friend constexpr bool operator!=(const It& l, const It& r) noexcept { return l.m_current != r.m_current; }
    };

    It begin() noexcept { return {m_pHead}; }
    It end() noexcept { return {}; }

    const It begin() const noexcept { return {m_pHead}; }
    const It end() const  noexcept { return {}; }

    /* */
protected:
    Node* insertNode(Node* pNew); /* prepend */
};

template<typename T>
inline typename SList<T>::Node*
SList<T>::Node::alloc(IAllocator* pAlloc, const T& x)
{
    Node* pNew = static_cast<Node*>(pAlloc->zalloc(1, sizeof(Node)));
    new(&pNew->data) T {x};
    return pNew;
}

template<typename T>
inline typename SList<T>::Node*
SList<T>::insert(IAllocator* pAlloc, const T& x)
{
    return insertNode(Node::alloc(pAlloc, x));
}

template<typename T>
inline typename SList<T>::Node*
SList<T>::insertNode(Node* pNew)
{
    pNew->pNext = m_pHead;
    m_pHead = pNew;

    return pNew;
}

template<typename T>
inline typename SList<T>::Node*
SList<T>::insert(Node* pNode)
{
    insertNode(pNode);
}

template<typename T>
inline void
SList<T>::remove(Node* pNode)
{
    for (Node* curr = m_pHead, * prev = nullptr; curr; prev = curr, curr = curr->pNext)
    {
        if (curr == pNode)
        {
            if (prev) prev->pNext = curr->pNext;
            else m_pHead = curr->pNext;

            break;
        }
    }
}

template<typename T>
inline void
SList<T>::remove(IAllocator* pAlloc, Node* pNode)
{
    remove(pNode);
    pAlloc->free(pNode);
}

template<typename T>
inline void
SList<T>::remove(Node* pPrev, Node* pNode)
{
    ADT_ASSERT(m_pHead != nullptr, "head: '{}'", m_pHead);

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

template<typename T>
inline void
SList<T>::remove(IAllocator* pAlloc, Node* pPrev, Node* pNode)
{
    remove(pPrev, pNode);
    pAlloc->free(pNode);
}

template<typename T>
inline void
SList<T>::destroy(IAllocator* pAlloc) noexcept
{
    for (
        Node* curr = m_pHead, * tmp = nullptr;
        curr && (tmp = curr->pNext, true);
        curr = tmp
    )
    {
        pAlloc->free(curr);
    }

    *this = {};
}

template<typename T>
inline SList<T>
SList<T>::release() noexcept
{
    return utils::exchange(this, {});
}

template<typename T, typename ALLOC_T = StdAllocatorNV>
struct SListManaged : SList<T>
{
    using Base = SList<T>;
    using Node = Base::Node;

    /* */

    ADT_NO_UNIQUE_ADDRESS ALLOC_T m_alloc;

    /* */

    SListManaged() = default;

    /* */

    Node* insert(const T& x) { return Base::insert(&m_alloc, x); }

    void remove(Node* pNode) { Base::remove(&m_alloc, pNode); }
    void remove(Node* pPrev, Node* pNode) { Base::remove(&m_alloc, pPrev, pNode); }

    void destroy() noexcept { Base::destroy(&m_alloc); }

    SListManaged release() noexcept { return utils::exchange(this, {}); }
};

} /* namespace adt */

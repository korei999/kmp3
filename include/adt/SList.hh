#pragma once

#include "Gpa.hh"

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

        template<typename ...ARGS>
        [[nodiscard("leak")]] static Node* alloc(IAllocator* pAlloc, ARGS&&... args);
    };

    /* */

    Node* m_pHead {};

    /* */

    Node* data() noexcept { return m_pHead; }
    const Node* data() const noexcept { return m_pHead; }

    bool empty() const noexcept { return m_pHead == nullptr; }

    Node* insert(IAllocator* pAlloc, const T& x); /* prepend */
    Node* insert(IAllocator* pAlloc, T&& x); /* prepend */
    Node* insert(Node* pNode); /* prepend */

    template<typename ...ARGS>
    Node* insertAfter(IAllocator* pAlloc, Node* pAfter, ARGS&&... args);

    Node* insertAfter(Node* pAfter, Node* pNew) noexcept;

    template<typename ...ARGS>
    Node* emplace(IAllocator* pAlloc, ARGS&&... args); /* prepend */

    void remove(Node* pNode); /* O(n) */
    void remove(IAllocator* pAlloc, Node* pNode); /* O(n) */
    void remove(Node* pPrev, Node* pNode); /* O(1) */
    void remove(IAllocator* pAlloc, Node* pPrev, Node* pNode); /* O(1); free(pNode) */

    void destroy(IAllocator* pAlloc) noexcept;
    void destructElements() noexcept;

    template<typename CL_DELETER>
    void destroy(IAllocator* pAlloc, CL_DELETER cl) noexcept;

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

        It current() noexcept { return m_current; }
        const It current() const noexcept { return m_current; }

        It next() noexcept { return m_current->pNext; }
        const It next() const noexcept { return m_current->pNext; }

        friend constexpr bool operator==(const It& l, const It& r) noexcept { return l.m_current == r.m_current; }
        friend constexpr bool operator!=(const It& l, const It& r) noexcept { return l.m_current != r.m_current; }
    };

    It begin() noexcept { return {m_pHead}; }
    It end() noexcept { return {}; }

    const It begin() const noexcept { return {m_pHead}; }
    const It end() const noexcept { return {}; }

    /* */
protected:
    Node* insertNode(Node* pNew); /* prepend */
};

template<typename T>
template<typename ...ARGS>
inline typename SList<T>::Node*
SList<T>::Node::alloc(IAllocator* pAlloc, ARGS&&... args)
{
    return pAlloc->alloc<Node>(nullptr, std::forward<ARGS>(args)...);
}

template<typename T>
inline typename SList<T>::Node*
SList<T>::insert(IAllocator* pAlloc, const T& x)
{
    return insertNode(Node::alloc(pAlloc, x));
}

template<typename T>
inline typename SList<T>::Node*
SList<T>::insert(IAllocator* pAlloc, T&& x)
{
    return emplace(pAlloc, std::move(x));
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
    return insertNode(pNode);
}

template<typename T>
template<typename ...ARGS>
inline typename SList<T>::Node*
SList<T>::insertAfter(IAllocator* pAlloc, Node* pAfter, ARGS&&... args)
{
    return insertAfter(pAfter, Node::alloc(pAlloc, std::forward<ARGS>(args)...));
}

template<typename T>
inline typename SList<T>::Node*
SList<T>::insertAfter(Node* pAfter, Node* pNew) noexcept
{
    Node* pNext = pAfter->pNext;
    pAfter->pNext = pNew;
    pNew->pNext = pNext;
}

template<typename T>
template<typename ...ARGS>
inline typename SList<T>::Node*
SList<T>::emplace(IAllocator* pAlloc, ARGS&&... args)
{
    static_assert(std::is_constructible_v<T, ARGS...>);

    return insertNode(Node::alloc(pAlloc, std::forward<ARGS>(args)...));
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
        pAlloc->dealloc(curr);
    }

    *this = {};
}

template<typename T>
inline void
SList<T>::destructElements() noexcept
{
    for (auto& e : *this)
        e.~T();
}

template<typename T>
template<typename CL_DELETER>
inline void
SList<T>::destroy(IAllocator* pAlloc, CL_DELETER cl) noexcept
{
    for (
        Node* curr = m_pHead, * tmp = nullptr;
        curr && (tmp = curr->pNext, true);
        curr = tmp
    )
    {
        cl(&curr->data);
        pAlloc->dealloc(curr);
    }

    *this = {};
}

template<typename T>
inline SList<T>
SList<T>::release() noexcept
{
    return utils::exchange(this, {});
}

template<typename T, typename ALLOC_T = GpaNV>
struct SListManaged : public SList<T>
{
    using Base = SList<T>;
    using Node = Base::Node;

    /* */

    SListManaged() = default;

    /* */

    auto* allocator() const { return ALLOC_T::inst(); }

    Node* insert(const T& x) { return Base::insert(allocator(), x); }

    template<typename ...ARGS>
    Node* insertAfter(Node* pAfter, ARGS&&... args) { return Base::insertAfter(allocator(), pAfter, std::forward<ARGS>(args)...); }

    template<typename ...ARGS>
    Node* emplace(ARGS&&... args) { return Base::emplace(allocator(), std::forward<ARGS>(args)...); }

    void remove(Node* pNode) { Base::remove(allocator(), pNode); }
    void remove(Node* pPrev, Node* pNode) { Base::remove(allocator(), pPrev, pNode); }

    void destroy() noexcept { Base::destroy(allocator()); }

    template<typename CL_DELETER>
    void destroy(CL_DELETER cl) noexcept { Base::destroy(allocator(), cl); }

    SListManaged release() noexcept { return utils::exchange(this, {}); }
};

template<typename T>
using SListM = SListManaged<T, GpaNV>;

} /* namespace adt */

#pragma once

#include "Gpa.hh"
#include "utils.hh"

#include <cstddef>

namespace adt
{

#define ADT_LIST_FOREACH_SAFE(L, IT, TMP) for (decltype((L)->m_pFirst) IT = (L)->m_pFirst, TMP = {}; IT && ((TMP) = (IT)->pNext, true); (IT) = (TMP))
#define ADT_LIST_FOREACH(L, IT) for (auto IT = (L)->m_pFirst; (IT); (IT) = (IT)->pNext)

template<typename T>
struct List
{
    struct Node
    {
        Node* pPrev;
        Node* pNext;
        ADT_NO_UNIQUE_ADDRESS T data;

        /* */

        template<typename ...ARGS>
        [[nodiscard("leak")]] static Node* alloc(IAllocator* pAlloc, ARGS&&... args);
    };

    /* */

    Node* m_pFirst {};
    Node* m_pLast {};

    /* */

    constexpr Node* data() noexcept { return m_pFirst; }
    constexpr const Node* data() const noexcept { return m_pFirst; }

    constexpr bool empty() const { return m_pFirst == nullptr || m_pLast == nullptr; }

    constexpr void destroy(IAllocator* pA) noexcept;
    constexpr void destructElements() noexcept;

    template<typename CL_DELETER>
    constexpr void destroy(IAllocator* pA, CL_DELETER cl) noexcept;

    constexpr List release() noexcept;

    constexpr Node* pushFront(Node* pNew);
    constexpr Node* pushBack(Node* pNew);

    constexpr Node* pushFront(IAllocator* pA, const T& x);
    constexpr Node* pushBack(IAllocator* pA, const T& x);

    constexpr Node* pushFront(IAllocator* pA, T&& x);
    constexpr Node* pushBack(IAllocator* pA, T&& x);

    template<typename ...ARGS>
    constexpr Node* emplaceFront(IAllocator* p, ARGS&&... args);

    template<typename ...ARGS>
    constexpr Node* emplaceBack(IAllocator* p, ARGS&&... args);

    constexpr void remove(Node* p);

    void remove(T* p);
    void remove(IAllocator* pAlloc, T* p);

    constexpr void insertAfter(Node* pAfter, Node* p);
    constexpr void insertBefore(Node* pBefore, Node* p);

    template<auto FN_CMP = utils::compare<T>>
    constexpr void sort();

    /* */

    struct It
    {
        Node* pNode = nullptr;

        It() = default;
        It(Node* p) : pNode {p} {}

        T& operator*() { return pNode->data; }
        T* operator->() { return &pNode->data; }

        It operator++() { return pNode = pNode->pNext; }
        It operator++(int) { T* tmp = pNode++; return tmp; }

        It operator--() { return pNode = pNode->pPrev; }
        It operator--(int) { T* tmp = pNode--; return tmp; }

        It current() noexcept { return pNode; }
        const It current() const noexcept { return pNode; }

        It next() noexcept { return pNode->pNext; }
        const It next() const noexcept { return pNode->pNext; }

        friend constexpr bool operator==(const It l, const It r) { return l.pNode == r.pNode; }
        friend constexpr bool operator!=(const It l, const It r) { return l.pNode != r.pNode; }
    };

    It begin() { return {m_pFirst}; }
    It end() { return nullptr; }
    It rbegin() { return {m_pLast}; }
    It rend() { return nullptr; }

    const It begin() const { return {m_pFirst}; }
    const It end() const { return nullptr; }
    const It rbegin() const { return {m_pLast}; }
    const It rend() const { return nullptr; }
};

template<typename T>
template<typename ...ARGS>
inline typename List<T>::Node*
List<T>::Node::alloc(IAllocator* pAlloc, ARGS&&... args)
{
    return pAlloc->alloc<Node>(nullptr, nullptr, std::forward<ARGS>(args)...);
}

template<typename T>
constexpr void
List<T>::destroy(IAllocator* pA) noexcept
{
    ADT_LIST_FOREACH_SAFE(this, it, tmp)
        pA->dealloc(it);

    *this = {};
}

template<typename T>
constexpr void
List<T>::destructElements() noexcept
{
    for (auto& e : *this)
        e.~T();
}

template<typename T>
template<typename CL_DELETER>
inline constexpr void
List<T>::destroy(IAllocator* pA, CL_DELETER cl) noexcept
{
    ADT_LIST_FOREACH_SAFE(this, it, tmp)
    {
        cl(&it->data);
        pA->free(it);
    }

    *this = {};
}

template<typename T>
constexpr List<T>
List<T>::release() noexcept
{
    return utils::exchange(this, {});
}

template<typename T>
constexpr List<T>::Node*
List<T>::pushFront(Node* pNew)
{
    if (!m_pFirst)
    {
        pNew->pNext = pNew->pPrev = nullptr;
        m_pLast = m_pFirst = pNew;
    }
    else
    {
        pNew->pPrev = nullptr;
        pNew->pNext = m_pFirst;
        m_pFirst->pPrev = pNew;
        m_pFirst = pNew;
    }

    return pNew;
}

template<typename T>
constexpr List<T>::Node*
List<T>::pushBack(Node* pNew)
{
    if (!m_pFirst)
    {
        pNew->pNext = pNew->pPrev = nullptr;
        m_pLast = m_pFirst = pNew;
    }
    else
    {
        pNew->pNext = nullptr;
        pNew->pPrev = m_pLast;
        m_pLast->pNext = pNew;
        m_pLast = pNew;
    }

    return pNew;
}

template<typename T>
constexpr List<T>::Node*
List<T>::pushFront(IAllocator* pA, const T& x)
{
    auto* pNew = Node::alloc(pA, x);
    return pushFront(pNew);
}

template<typename T>
constexpr List<T>::Node*
List<T>::pushBack(IAllocator* pA, const T& x)
{
    auto* pNew = Node::alloc(pA, x);
    return pushBack(pNew);
}

template<typename T>
constexpr List<T>::Node*
List<T>::pushFront(IAllocator* pA, T&& x)
{
    auto* pNew = Node::alloc(pA, std::move(x));
    return pushFront(pNew);
}

template<typename T>
constexpr List<T>::Node*
List<T>::pushBack(IAllocator* pA, T&& x)
{
    auto* pNew = Node::alloc(pA, std::move(x));
    return pushBack(pNew);
}

template<typename T>
template<typename ...ARGS>
constexpr List<T>::Node*
List<T>::emplaceFront(IAllocator* p, ARGS&&... args)
{
    return pushFront(Node::alloc(p, std::forward<ARGS>(args)...));
}

template<typename T>
template<typename ...ARGS>
constexpr List<T>::Node*
List<T>::emplaceBack(IAllocator* p, ARGS&&... args)
{
    return pushBack(Node::alloc(p, std::forward<ARGS>(args)...));
}

template<typename T>
constexpr void
List<T>::remove(Node* p)
{
    ADT_ASSERT(p != nullptr, "");

    if (p == m_pFirst && p == m_pLast)
    {
        m_pFirst = m_pLast = nullptr;
    }
    else if (p == m_pFirst)
    {
        m_pFirst = m_pFirst->pNext;
        m_pFirst->pPrev = nullptr;
    }
    else if (p == m_pLast)
    {
        m_pLast = m_pLast->pPrev;
        m_pLast->pNext = nullptr;
    }
    else 
    {
        p->pPrev->pNext = p->pNext;
        p->pNext->pPrev = p->pPrev;
    }
}

template<typename T>
inline void
List<T>::remove(T* p)
{
    Node* pNode = (u8*)(p) - sizeof(void*)*2;
    remove(pNode);
}

template<typename T>
inline void
List<T>::remove(IAllocator* pAlloc, T* p)
{
    Node* pNode = (Node*)((u8*)(p) - offsetof(Node, data));
    remove(pNode);
    pAlloc->free(pNode);
}

template<typename T>
constexpr void
List<T>::insertAfter(Node* pAfter, Node* p)
{
    p->pPrev = pAfter;
    p->pNext = pAfter->pNext;

    if (p->pNext) p->pNext->pPrev = p;

    pAfter->pNext = p;

    if (pAfter == m_pLast) m_pLast = p;
}

template<typename T>
constexpr void
List<T>::insertBefore(Node* pBefore, Node* p)
{
    p->pNext = pBefore;
    p->pPrev = pBefore->pPrev;

    if (p->pPrev) p->pPrev->pNext = p;

    pBefore->pPrev = p;

    if (pBefore == m_pFirst) m_pFirst = p;
}

/* https://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.c */
template<typename T>
template<auto FN_CMP>
constexpr void
List<T>::sort()
{
    Node* p {}, * q {}, * e {}, * tail {};
    long inSize {}, nMerges {}, pSize {}, qSize {}, i {};
    Node* list = m_pFirst;

    if (!m_pFirst)
        return;

    inSize = 1;

    while (true)
    {
        p = list;
        list = tail = nullptr;
        nMerges = 0; /* count number of merges we do in this pass */

        while (p)
        {
            ++nMerges; /* there exists a merge to be done */
            /* step `insize` places along from p */
            q = p;
            pSize = 0;
            for (i = 0; i < inSize; ++i)
            {
                ++pSize;
                q = q->pNext;
                if (!q) break;
            }

            /* if q hasn't fallen off end, we have two lists to merge */
            qSize = inSize;

            /* now we have two lists; merge them */
            while (pSize > 0 || (qSize > 0 && q))
            {
                /* decide whether next element of merge comes from p or q */
                if (pSize == 0)
                {
                    /* p is empty; e must come from q. */
                    e = q, q = q->pNext, --qSize;
                }
                else if (qSize == 0 || !q)
                {
                    /* q is empty; e must come from p. */
                    e = p, p = p->pNext, --pSize;
                }
                else if (FN_CMP(p->data, q->data) <= 0)
                {
                    /* First element of p is lower (or same), e must come from p. */
                    e = p, p = p->pNext, --pSize;
                }
                else
                {
                    /* First element of q is lower, e must come from q. */
                    e = q, q = q->pNext, --qSize;
                }

                /* add the next element to the merged list */
                if (tail) tail->pNext = e;
                else list = e;

                /* maintain reverse pointes in a doubly linked list */
                e->pPrev = tail;
                tail = e;
            }

            /* now p has stepped `insize` places along , and q has too */
            p = q;
        }

        tail->pNext = nullptr;

        /* if we have done only one merge, we're finished. */
        if (nMerges <= 1) /* allow for nMerges == 0, the empty list case */
            break;

        /* otherwise repeat, merging lists twice the size */
        inSize *= 2;
    }

    m_pFirst = list;
    m_pLast = tail;
}

template<typename T, typename ALLOC_T = GpaNV>
struct ListManaged : public List<T>
{
    using Base = List<T>;
    using Node = Base::Node;

    /* */

    ListManaged() = default;

    /* */

    auto* allocator() const { return ALLOC_T::inst(); }

    [[nodiscard]] constexpr bool empty() const { return Base::empty(); }

    constexpr Node* pushFront(const T& x) { return Base::pushFront(allocator(), x); }
    constexpr Node* pushBack(const T& x) { return Base::pushBack(allocator(), x); }

    constexpr void remove(Node* p) { Base::remove(p); allocator()->free(p); }

    constexpr void destroy() { Base::destroy(allocator()); }

    template<typename CL_DELETER>
    constexpr void destroy(CL_DELETER cl) noexcept { Base::destroy(allocator(), cl); }

    constexpr ListManaged release() noexcept { return utils::exchange(this, {}); }

    template<auto FN_CMP = utils::compare<T>>
    constexpr void sort() { Base::template sort<FN_CMP>(); }
};

template<typename T>
using ListM = ListManaged<T>;

} /* namespace adt */

#pragma once

#include "StdAllocator.hh"
#include "utils.hh"

#include <cstddef>

namespace adt
{

#define ADT_LIST_FOREACH_SAFE(L, IT, TMP) for (decltype((L)->m_pFirst) IT = (L)->m_pFirst, TMP = {}; IT && ((TMP) = (IT)->pNext, true); (IT) = (TMP))
#define ADT_LIST_FOREACH(L, IT) for (auto IT = (L)->m_pFirst; (IT); (IT) = (IT)->pNext)

template<typename T>
struct ListNode
{
    ListNode* pPrev;
    ListNode* pNext;
    ADT_NO_UNIQUE_ADDRESS T data;
};

template<typename T>
constexpr ListNode<T>*
ListNodeAlloc(IAllocator* pA, const T& x)
{
    auto* pNew = (ListNode<T>*)pA->alloc<ListNode<T>>();
    new(&pNew->data) T(x);

    return pNew;
}

template<typename T, typename ...ARGS>
constexpr ListNode<T>*
ListNodeAlloc(IAllocator* pA, ARGS&&... args)
{
    auto* pNew = (ListNode<T>*)pA->alloc<ListNode<T>>();
    new(&pNew->data) T {std::forward<ARGS>(args)...};

    return pNew;
}

template<typename T>
struct List
{
    ListNode<T>* m_pFirst {};
    ListNode<T>* m_pLast {};
    isize m_size {};

    /* */

    [[nodiscard]] constexpr isize size() const { return m_size; }

    [[nodiscard]] constexpr bool empty() const { return m_size <= 0; }

    constexpr void destroy(IAllocator* pA) noexcept;
    constexpr void destructElements() noexcept;

    constexpr List release() noexcept;

    constexpr ListNode<T>* pushFront(ListNode<T>* pNew);
    constexpr ListNode<T>* pushBack(ListNode<T>* pNew);

    constexpr ListNode<T>* pushFront(IAllocator* pA, const T& x);
    constexpr ListNode<T>* pushBack(IAllocator* pA, const T& x);

    constexpr ListNode<T>* pushFront(IAllocator* pA, T&& x);
    constexpr ListNode<T>* pushBack(IAllocator* pA, T&& x);

    template<typename ...ARGS>
    constexpr ListNode<T>* emplaceFront(IAllocator* p, ARGS&&... args);

    template<typename ...ARGS>
    constexpr ListNode<T>* emplaceBack(IAllocator* p, ARGS&&... args);

    constexpr void remove(ListNode<T>* p);

    void remove(T* p);
    void remove(IAllocator* pAlloc, T* p);

    constexpr void insertAfter(ListNode<T>* pAfter, ListNode<T>* p);
    constexpr void insertBefore(ListNode<T>* pBefore, ListNode<T>* p);

    template<auto FN_CMP = utils::compare<T>>
    constexpr void sort();

    /* */

    struct It
    {
        ListNode<T>* s = nullptr;

        It(ListNode<T>* p) : s {p} {}

        T& operator*() { return s->data; }
        T* operator->() { return &s->data; }

        It operator++() { return s = s->pNext; }
        It operator++(int) { T* tmp = s++; return tmp; }

        It operator--() { return s = s->pPrev; }
        It operator--(int) { T* tmp = s--; return tmp; }

        friend constexpr bool operator==(const It l, const It r) { return l.s == r.s; }
        friend constexpr bool operator!=(const It l, const It r) { return l.s != r.s; }
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
constexpr List<T>
List<T>::release() noexcept
{
    return utils::exchange(this, {});
}

template<typename T>
constexpr ListNode<T>*
List<T>::pushFront(ListNode<T>* pNew)
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

    ++m_size;
    return pNew;
}

template<typename T>
constexpr ListNode<T>*
List<T>::pushBack(ListNode<T>* pNew)
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

    ++m_size;
    return pNew;
}

template<typename T>
constexpr ListNode<T>*
List<T>::pushFront(IAllocator* pA, const T& x)
{
    auto* pNew = ListNodeAlloc(pA, x);
    return pushFront(pNew);
}

template<typename T>
constexpr ListNode<T>*
List<T>::pushBack(IAllocator* pA, const T& x)
{
    auto* pNew = ListNodeAlloc(pA, x);
    return pushBack(pNew);
}

template<typename T>
constexpr ListNode<T>*
List<T>::pushFront(IAllocator* pA, T&& x)
{
    auto* pNew = ListNodeAlloc(pA, std::move(x));
    return pushFront(pNew);
}

template<typename T>
constexpr ListNode<T>*
List<T>::pushBack(IAllocator* pA, T&& x)
{
    auto* pNew = ListNodeAlloc(pA, std::move(x));
    return pushBack(pNew);
}

template<typename T>
template<typename ...ARGS>
constexpr ListNode<T>*
List<T>::emplaceFront(IAllocator* p, ARGS&&... args)
{
    return pushFront(ListNodeAlloc(p, std::forward<ARGS>(args)...));
}

template<typename T>
template<typename ...ARGS>
constexpr ListNode<T>*
List<T>::emplaceBack(IAllocator* p, ARGS&&... args)
{
    return pushBack(ListNodeAlloc(p, std::forward<ARGS>(args)...));
}

template<typename T>
constexpr void
List<T>::remove(ListNode<T>* p)
{
    ADT_ASSERT(p && m_size > 0, "");

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

    --m_size;
}

template<typename T>
inline void
List<T>::remove(T* p)
{
    ListNode<T>* pNode = (u8*)(p) - sizeof(void*)*2;
    remove(pNode);
}

template<typename T>
inline void
List<T>::remove(IAllocator* pAlloc, T* p)
{
    ListNode<T>* pNode = (ListNode<T>*)((u8*)(p) - offsetof(ListNode<T>, data));
    remove(pNode);
    pAlloc->free(pNode);
}

template<typename T>
constexpr void
List<T>::insertAfter(ListNode<T>* pAfter, ListNode<T>* p)
{
    p->pPrev = pAfter;
    p->pNext = pAfter->pNext;

    if (p->pNext) p->pNext->pPrev = p;

    pAfter->pNext = p;

    if (pAfter == m_pLast) m_pLast = p;

    ++m_size;
}

template<typename T>
constexpr void
List<T>::insertBefore(ListNode<T>* pBefore, ListNode<T>* p)
{
    p->pNext = pBefore;
    p->pPrev = pBefore->pPrev;

    if (p->pPrev) p->pPrev->pNext = p;

    pBefore->pPrev = p;

    if (pBefore == m_pFirst) m_pFirst = p;

    ++m_size;
}

/* https://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.c */
template<typename T>
template<auto FN_CMP>
constexpr void
List<T>::sort()
{
    ListNode<T>* p {}, * q {}, * e {}, * tail {};
    long inSize {}, nMerges {}, pSize {}, qSize {}, i {};
    ListNode<T>* list = m_pFirst;

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

template<typename T, typename ALLOC_T = StdAllocatorNV>
struct ListManaged : protected ALLOC_T, public List<T>
{
    using Base = List<T>;

    /* */

    ListManaged() = default;

    /* */

    ALLOC_T& allocator() { return static_cast<ALLOC_T&>(*this); }
    const ALLOC_T& allocator() const { return static_cast<ALLOC_T&>(*this); }

    [[nodiscard]] constexpr isize size() const { return Base::size(); }

    [[nodiscard]] constexpr bool empty() const { return Base::empty(); }

    constexpr ListNode<T>* pushFront(const T& x) { return Base::pushFront(&allocator(), x); }

    constexpr ListNode<T>* pushBack(const T& x) { return Base::pushBack(&allocator(), x); }

    constexpr void remove(ListNode<T>* p) { Base::remove(p); allocator().free(p); }

    constexpr void destroy() { Base::destroy(&allocator()); }

    constexpr ListManaged release() noexcept { return utils::exchange(this, {}); }

    constexpr void insertAfter(ListNode<T>* pAfter, ListNode<T>* p) { Base::insertAfter(pAfter, p); }

    constexpr void insertBefore(ListNode<T>* pBefore, ListNode<T>* p) { Base::insertBefore(pBefore, p); }

    template<auto FN_CMP = utils::compare<T>>
    constexpr void sort() { Base::template sort<FN_CMP>(); }
};

template<typename T>
using ListM = ListManaged<T>;

} /* namespace adt */

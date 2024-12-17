#pragma once

#include "IAllocator.hh"
#include "print.hh"

namespace adt
{

#define ADT_LIST_FOREACH_SAFE(L, IT, TMP) for (decltype((L)->pFirst) IT = (L)->pFirst, TMP = {}; IT && ((TMP) = (IT)->pNext, true); (IT) = (TMP))
#define ADT_LIST_FOREACH(L, IT) for (auto IT = (L)->pFirst; (IT); (IT) = (IT)->pNext)

template<typename T>
struct ListNode
{
    ListNode* pPrev;
    ListNode* pNext;
    T data;
};

template<typename T>
constexpr ListNode<T>*
ListNodeAlloc(IAllocator* pA, const T& x)
{
    auto* pNew = (ListNode<T>*)pA->alloc(1, sizeof(ListNode<T>));
    pNew->data = x;

    return pNew;
}

template<typename T>
struct ListBase
{
    ListNode<T>* pFirst {};
    ListNode<T>* pLast {};
    u32 size {};

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

    It begin() { return {this->pFirst}; }
    It end() { return nullptr; }
    It rbegin() { return {this->pLast}; }
    It rend() { return nullptr; }

    const It begin() const { return {this->pFirst}; }
    const It end() const { return nullptr; }
    const It rbegin() const { return {this->pLast}; }
    const It rend() const { return nullptr; }

    constexpr void destroy(IAllocator* pA);

    constexpr ListNode<T>* pushFront(ListNode<T>* pNew);

    constexpr ListNode<T>* pushBack(ListNode<T>* pNew);

    constexpr ListNode<T>* pushFront(IAllocator* pA, const T& x);

    constexpr ListNode<T>* pushBack(IAllocator* pA, const T& x);

    constexpr void remove(ListNode<T>* p);

    constexpr void insertAfter(ListNode<T>* pAfter, ListNode<T>* p);

    constexpr void insertBefore(ListNode<T>* pBefore, ListNode<T>* p);

    template<auto FN_CMP = utils::compare<T>>
    constexpr void sort();

};

template<typename T>
constexpr void
ListBase<T>::destroy(IAllocator* pA)
{
    ADT_LIST_FOREACH_SAFE(this, it, tmp)
        pA->free(it);

    this->pFirst = this->pLast = nullptr;
    this->size = 0;
}

template<typename T>
constexpr ListNode<T>*
ListBase<T>::pushFront(ListNode<T>* pNew)
{
    if (!this->pFirst)
    {
        pNew->pNext = pNew->pPrev = nullptr;
        this->pLast = this->pFirst = pNew;
    }
    else
    {
        pNew->pPrev = nullptr;
        pNew->pNext = this->pFirst;
        this->pFirst->pPrev = pNew;
        this->pFirst = pNew;
    }

    ++this->size;
    return pNew;
}

template<typename T>
constexpr ListNode<T>*
ListBase<T>::pushBack(ListNode<T>* pNew)
{
    if (!this->pFirst)
    {
        pNew->pNext = pNew->pPrev = nullptr;
        this->pLast = this->pFirst = pNew;
    }
    else
    {
        pNew->pNext = nullptr;
        pNew->pPrev = this->pLast;
        this->pLast->pNext = pNew;
        this->pLast = pNew;
    }

    ++this->size;
    return pNew;
}

template<typename T>
constexpr ListNode<T>*
ListBase<T>::pushFront(IAllocator* pA, const T& x)
{
    auto* pNew = ListNodeAlloc(pA, x);
    return this->pushFront(pNew);
}

template<typename T>
constexpr ListNode<T>*
ListBase<T>::pushBack(IAllocator* pA, const T& x)
{
    auto* pNew = ListNodeAlloc(pA, x);
    return this->pushBack(pNew);
}

template<typename T>
constexpr void
ListBase<T>::remove(ListNode<T>* p)
{
    assert(p && this->size > 0);

    if (p == this->pFirst && p == this->pLast)
    {
        this->pFirst = this->pLast = nullptr;
    }
    else if (p == this->pFirst)
    {
        this->pFirst = this->pFirst->pNext;
        this->pFirst->pPrev = nullptr;
    }
    else if (p == this->pLast)
    {
        this->pLast = this->pLast->pPrev;
        this->pLast->pNext = nullptr;
    }
    else 
    {
        p->pPrev->pNext = p->pNext;
        p->pNext->pPrev = p->pPrev;
    }

    --this->size;
}

template<typename T>
constexpr void
ListBase<T>::insertAfter(ListNode<T>* pAfter, ListNode<T>* p)
{
    p->pPrev = pAfter;
    p->pNext = pAfter->pNext;

    if (p->pNext) p->pNext->pPrev = p;

    pAfter->pNext = p;

    if (pAfter == this->pLast) this->pLast = p;

    ++this->size;
}

template<typename T>
constexpr void
ListBase<T>::insertBefore(ListNode<T>* pBefore, ListNode<T>* p)
{
    p->pNext = pBefore;
    p->pPrev = pBefore->pPrev;

    if (p->pPrev) p->pPrev->pNext = p;

    pBefore->pPrev = p;

    if (pBefore == this->pFirst) this->pFirst = p;

    ++this->size;
}

/* https://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.c */
template<typename T>
template<auto FN_CMP>
constexpr void
ListBase<T>::sort()
{
    ListNode<T>* p, * q, * e, * tail;
    long inSize, nMerges, pSize, qSize, i;
    ListNode<T>* list = this->pFirst;

    if (!this->pFirst) return;

    inSize = 1;

    while (true)
    {
        p = list;
        list = nullptr;
        tail = nullptr;
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

    this->pFirst = list;
    this->pLast = tail;
}

template<typename T>
struct List
{
    ListBase<T> base {};
    IAllocator* pAlloc {};

    List() = default;
    List(IAllocator* pA) : pAlloc(pA) {}

    ListBase<T>::It begin() { return base.begin(); }
    ListBase<T>::It end() { return base.end(); }
    ListBase<T>::It rbegin() { return base.rbegin(); }
    ListBase<T>::It rend() { return base.rend(); }

    const ListBase<T>::It begin() const { return base.begin(); }
    const ListBase<T>::It end() const { return base.end(); }
    const ListBase<T>::It rbegin() const { return base.rbegin(); }
    const ListBase<T>::It rend() const { return base.rend(); }

    constexpr ListNode<T>* pushFront(const T& x) { return base.pushFront(pAlloc, x); }

    constexpr ListNode<T>* pushBack(const T& x) { return base.pushBack(pAlloc, x); }

    constexpr void remove(ListNode<T>* p) { base.remove(p); pAlloc->free(p); }

    constexpr void destroy() { base.destroy(pAlloc); }

    constexpr void insertAfter(ListNode<T>* pAfter, ListNode<T>* p) { base.insertAfter(pAfter, p); }

    constexpr void insertBefore(ListNode<T>* pBefore, ListNode<T>* p) { base.insertBefore(pBefore, p); }

    template<auto FN_CMP = utils::compare<T>>
    constexpr void sort() { base.template sort<FN_CMP>(); }
};

namespace utils
{

template<typename T> [[nodiscard]] inline bool empty(const ListBase<T>* s) { return s->size == 0; }
template<typename T> [[nodiscard]] inline bool empty(const List<T>* s) { return empty(&s->base); }

} /* namespace utils */

namespace print
{

template<typename T>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const ListBase<T>& x)
{
    if (utils::empty(&x))
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "(empty)");
    }

    char aBuff[1024] {};
    u32 nRead = 0;
    ADT_LIST_FOREACH(&x, it)
    {
        const char* fmt;
        if constexpr (std::is_floating_point_v<T>) fmt = it == x.pLast ? "{:.3}" : "{:.3}, ";
        else fmt = it == x.pLast ? "{}" : "{}, ";

        nRead += toBuffer(aBuff + nRead, utils::size(aBuff) - nRead, fmt, it->data);
    }

    return print::copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

template<typename T>
inline u32
formatToContext(Context ctx, FormatArgs fmtArgs, const List<T>& x)
{
    return formatToContext(ctx, fmtArgs, x.base);
}

} /* namespace print */

} /* namespace adt */

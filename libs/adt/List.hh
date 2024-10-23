#pragma once

#include "Allocator.hh"

namespace adt
{

#define ADT_LIST_FOREACH_SAFE(L, IT, TMP) for (decltype((L)->pFirst) IT = (L)->pFirst, TMP = {}; IT && ((TMP) = (IT)->pNext, true); (IT) = (TMP))

template<typename T>
struct ListNode
{
    ListNode* pPrev;
    ListNode* pNext;
    T data;
};

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
};

template<typename T>
constexpr ListNode<T>*
ListNodeAlloc(Allocator* pA, const T& x)
{
    auto* pNew = (ListNode<T>*)alloc(pA, 0, sizeof(T));
    pNew->data = x;

    return pNew;
}

template<typename T>
constexpr ListNode<T>*
ListPushBack(ListBase<T>* s, Allocator* pA, const T& x)
{
    auto* pNew = ListNodeAlloc(pA, x);
    return ListPushBack(s, pNew);
}

template<typename T>
constexpr ListNode<T>*
ListPushBack(ListBase<T>* s, ListNode<T>* pNew)
{
    if (!s->pFirst)
    {
        s->pLast = s->pFirst = pNew;
        s->pFirst->pPrev = s->pFirst->pNext = nullptr;
        goto done;
    }

    s->pLast->pNext = pNew;
    pNew->pPrev = s->pLast;
    pNew->pNext = nullptr;

    s->pLast = pNew;

done:
    s->size++;
    return pNew;
}

template<typename T>
constexpr void
ListRemove(ListBase<T>* s, ListNode<T>* p)
{
    assert(p && s->size > 0);

    if (p == s->pFirst && p == s->pLast)
    {
        s->pFirst = s->pLast = nullptr;
    }
    else if (p == s->pFirst)
    {
        s->pFirst = s->pFirst->pNext;
        s->pFirst->pPrev = nullptr;
    }
    else if (p == s->pLast)
    {
        s->pLast = s->pLast->pPrev;
        s->pLast->pNext = nullptr;
    }
    else 
    {
        p->pPrev->pNext = p->pNext;
        p->pNext->pPrev = p->pPrev;
    }

    s->size--;
}

template<typename T>
constexpr void
ListDestroy(ListBase<T>* s, Allocator* pA)
{
    ADT_LIST_FOREACH_SAFE(s, it, tmp)
        free(pA, it);

    s->pFirst = s->pLast = nullptr;
}

template<typename T>
struct List
{
    ListBase<T> base {};
    Allocator* pAlloc {};

    List() = default;
    List(Allocator* pA) : pAlloc(pA) {}

    ListBase<T>::It begin() { return base.begin(); }
    ListBase<T>::It end() { return base.end(); }
    ListBase<T>::It rbegin() { return base.rbegin(); }
    ListBase<T>::It rend() { return rend(); }

    const ListBase<T>::It begin() const { return base.begin(); }
    const ListBase<T>::It end() const { return base.end(); }
    const ListBase<T>::It rbegin() const { return base.rbegin(); }
    const ListBase<T>::It rend() const { return rend(); }
};

template<typename T>
constexpr ListNode<T>*
ListPushBack(List<T>* s, const T& x)
{
    auto* pNew = ListNodeAlloc(s->pAlloc, x);
    return ListPushBack(&s->base, pNew);
}

template<typename T>
constexpr void
ListRemove(List<T>* s, ListNode<T>* p)
{
    ListRemove(&s->base, p);
    free(s->pAlloc, p);
}

template<typename T>
constexpr void
ListDestroy(List<T>* s)
{
    ListDestroy(&s->base, s->pAlloc);
}

} /* namespace adt */

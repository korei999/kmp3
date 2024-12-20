#pragma once

#include "IAllocator.hh"
#include "Vec.hh"
#include "utils.hh"
#include "sort.hh"

namespace adt
{

template<typename T>
struct Heap
{
    VecBase<T> m_vec {};

    /* */

    Heap() = default;
    Heap(IAllocator* pA, u32 prealloc = SIZE_MIN)
        : m_vec(pA, prealloc) {}

    /* */

    void destroy(IAllocator* pA);
    void minBubbleUp(u32 i);
    void maxBubbleUp(u32 i);
    void minBubbleDown(u32 i);
    void maxBubbleDown(u32 i);
    void pushMin(IAllocator* pA, const T& x);
    void pushMax(IAllocator* pA, const T& x);
    [[nodiscard]] T minExtract();
    [[nodiscard]] T maxExtract();
    void minSort(IAllocator* pA, VecBase<T>* a);
    void maxSort(IAllocator* pA, VecBase<T>* a);
};

template<typename T>
Heap<T> HeapMinFromVec(IAllocator* pA, const VecBase<T>& a);

template<typename T>
Heap<T> HeapMaxFromVec(IAllocator* pA, const VecBase<T>& a);

template<typename T>
inline void
Heap<T>::destroy(IAllocator* pA)
{
    this->m_vec.destroy(pA);
}

template<typename T>
inline void
Heap<T>::minBubbleUp(u32 i)
{
again:
    if (HeapParentI(i) == NPOS) return;

    if (this->m_vec[HeapParentI(i)] > this->m_vec[i])
    {
        utils::swap(&this->m_vec[i], &this->m_vec[HeapParentI(i)]);
        i = HeapParentI(i);
        goto again;
    }
}

template<typename T>
inline void
Heap<T>::maxBubbleUp(u32 i)
{
again:
    if (HeapParentI(i) == NPOS) return;

    if (this->m_vec[HeapParentI(i)] < this->m_vec[i])
    {
        utils::swap(&this->m_vec[i], &this->m_vec[HeapParentI(i)]);
        i = HeapParentI(i);
        goto again;
    }
}

template<typename T>
inline void
Heap<T>::minBubbleDown(u32 i)
{
    long smallest, left, right;
    Vec<T>& a = this->m_vec;

again:
    left = HeapLeftI(i);
    right = HeapRightI(i);

    if (left < a.size && a[left] < a[i])
        smallest = left;
    else smallest = i;

    if (right < a.size && a[right] < a[smallest])
        smallest = right;

    if (smallest != (long)i)
    {
        utils::swap(&a[i], &a[smallest]);
        i = smallest;
        goto again;
    }
}

template<typename T>
inline void
Heap<T>::maxBubbleDown(u32 i)
{
    long largest, left, right;
    VecBase<T>& a = this->m_vec;

again:
    left = HeapLeftI(i);
    right = HeapRightI(i);

    if (left < a.m_size && a[left] > a[i])
        largest = left;
    else largest = i;

    if (right < a.m_size && a[right] > a[largest])
        largest = right;

    if (largest != (long)i)
    {
        utils::swap(&a[i], &a[largest]);
        i = largest;
        goto again;
    }
}

template<typename T>
inline void
Heap<T>::pushMin(IAllocator* pA, const T& x)
{
    this->m_vec.push(pA, x);
    this->minBubbleUp(this->m_vec.getSize() - 1);
}

template<typename T>
inline void
Heap<T>::pushMax(IAllocator* pA, const T& x)
{
    this->m_vec.push(pA, x);
    this->maxBubbleUp(this->m_vec.getSize() - 1);
}

template<typename T>
inline Heap<T>
HeapMinFromVec(IAllocator* pA, const VecBase<T>& a)
{
    Heap<T> q (pA, a.cap);
    q.m_vec.size = a.m_size;
    utils::copy(q.m_vec.pData, a.m_pData, a.m_size);

    for (long i = q.m_vec.size / 2; i >= 0; i--)
        q.minBubbleDown(i);

    return q;
}

template<typename T>
inline Heap<T>
HeapMaxFromVec(IAllocator* pA, const VecBase<T>& a)
{
    Heap<T> q (pA, a.cap);
    q.m_vec.size = a.m_size;
    utils::copy(q.m_vec.pData, a.m_pData, a.m_size);

    for (long i = q.m_vec.size / 2; i >= 0; i--)
        q.maxBubbleDown(i);

    return q;
}

template<typename T>
[[nodiscard]]
inline T
Heap<T>::minExtract()
{
    assert(this->m_vec.m_size > 0 && "empty heap");

    this->m_vec.swapWithLast(0);

    T min = *this->m_vec.pop();
    this->minBubbleDown(0);

    return min;
}

template<typename T>
[[nodiscard]]
inline T
Heap<T>::maxExtract()
{
    assert(this->m_vec.m_size > 0 && "empty heap");

    this->m_vec.swapWithLast(0);

    T max = *this->m_vec.pop();
    this->maxBubbleDown(0);

    return max;
}

template<typename T>
inline void
Heap<T>::minSort(IAllocator* pA, VecBase<T>* a)
{
    Heap<T> s = HeapMinFromVec(pA, *a);

    for (u32 i = 0; i < a->size; ++i)
        (*a)[i] = s.minExtract();

    s.destroy(pA);
}

template<typename T>
inline void
HeapMaxSort(IAllocator* pA, Vec<T>* a)
{
    Heap<T> s = HeapMaxFromVec(pA, *a);

    for (u32 i = 0; i < a->size; ++i)
        (*a)[i] = s.maxExtract();

    s.destroy(pA);
}

} /* namespace adt */

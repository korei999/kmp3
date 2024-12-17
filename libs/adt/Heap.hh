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
    VecBase<T> vec {};

    Heap() = default;
    Heap(IAllocator* pA, u32 prealloc = SIZE_MIN)
        : vec(pA, prealloc) {}

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
    this->vec.destroy(pA);
}

template<typename T>
inline void
Heap<T>::minBubbleUp(u32 i)
{
again:
    if (HeapParentI(i) == NPOS) return;

    if (this->vec[HeapParentI(i)] > this->vec[i])
    {
        utils::swap(&this->vec[i], &this->vec[HeapParentI(i)]);
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

    if (this->vec[HeapParentI(i)] < this->vec[i])
    {
        utils::swap(&this->vec[i], &this->vec[HeapParentI(i)]);
        i = HeapParentI(i);
        goto again;
    }
}

template<typename T>
inline void
Heap<T>::minBubbleDown(u32 i)
{
    long smallest, left, right;
    Vec<T>& a = this->vec;

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
    VecBase<T>& a = this->vec;

again:
    left = HeapLeftI(i);
    right = HeapRightI(i);

    if (left < a.size && a[left] > a[i])
        largest = left;
    else largest = i;

    if (right < a.size && a[right] > a[largest])
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
    this->vec.push(pA, x);
    this->minBubbleUp(this->vec.getSize() - 1);
}

template<typename T>
inline void
Heap<T>::pushMax(IAllocator* pA, const T& x)
{
    this->vec.push(pA, x);
    this->maxBubbleUp(this->vec.getSize() - 1);
}

template<typename T>
inline Heap<T>
HeapMinFromVec(IAllocator* pA, const VecBase<T>& a)
{
    Heap<T> q (pA, a.cap);
    q.vec.size = a.size;
    utils::copy(q.vec.pData, a.pData, a.size);

    for (long i = q.vec.size / 2; i >= 0; i--)
        q.minBubbleDown(i);

    return q;
}

template<typename T>
inline Heap<T>
HeapMaxFromVec(IAllocator* pA, const VecBase<T>& a)
{
    Heap<T> q (pA, a.cap);
    q.vec.size = a.size;
    utils::copy(q.vec.pData, a.pData, a.size);

    for (long i = q.vec.size / 2; i >= 0; i--)
        q.maxBubbleDown(i);

    return q;
}

template<typename T>
[[nodiscard]]
inline T
Heap<T>::minExtract()
{
    assert(this->vec.size > 0 && "empty heap");

    this->vec.swapWithLast(0);

    T min = *this->vec.pop();
    this->minBubbleDown(0);

    return min;
}

template<typename T>
[[nodiscard]]
inline T
Heap<T>::maxExtract()
{
    assert(this->vec.size > 0 && "empty heap");

    this->vec.swapWithLast(0);

    T max = *this->vec.pop();
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

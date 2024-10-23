#pragma once

#include "Allocator.hh"
#include "Vec.hh"
#include "utils.hh"
#include "sort.hh"

namespace adt
{

template<typename T>
struct Heap
{
    Vec<T> a {};

    Heap() = default;
    Heap(Allocator* pA, u32 prealloc = SIZE_MIN)
        : a {pA, prealloc} {}
};

template<typename T> inline void HeapDestroy(Heap<T>* s);
template<typename T> inline void HeapMinBubbleUp(Heap<T>* s, u32 i);
template<typename T> inline void HeapMaxBubbleUp(Heap<T>* s, u32 i);
template<typename T> inline void HeapMinBubbleDown(Heap<T>* s, u32 i);
template<typename T> inline void HeapMaxBubbleDown(Heap<T>* s, u32 i);
template<typename T> inline void HeapPushMin(Heap<T>* s, const T& x);
template<typename T> inline void HeapPushMax(Heap<T>* s, const T& x);
template<typename T> inline Heap<T> HeapMinFromVec(Allocator* pA, const Vec<T>& a);
template<typename T> inline Heap<T> HeapMaxFromVec(Allocator* pA, const Vec<T>& a);
template<typename T> [[nodiscard]] inline T HeapMinExtract(Heap<T>* s);
template<typename T> [[nodiscard]] inline T HeapMaxExtract(Heap<T>* s);
template<typename T> inline void HeapMinSort(Allocator* pA, Vec<T>* a);
template<typename T> inline void HeapMaxSort(Allocator* pA, Vec<T>* a);

template<typename T>
inline void
HeapDestroy(Heap<T>* s)
{
    VecDestroy(&s->a);
}

template<typename T>
inline void
HeapMinBubbleUp(Heap<T>* s, u32 i)
{
again:
    if (HeapParentI(i) == NPOS) return;

    if (s->a[HeapParentI(i)] > s->a[i])
    {
        utils::swap(&s->a[i], &s->a[HeapParentI(i)]);
        i = HeapParentI(i);
        goto again;
    }
}

template<typename T>
inline void
HeapMaxBubbleUp(Heap<T>* s, u32 i)
{
again:
    if (HeapParentI(i) == NPOS) return;

    if (s->a[HeapParentI(i)] < s->a[i])
    {
        utils::swap(&s->a[i], &s->a[HeapParentI(i)]);
        i = HeapParentI(i);
        goto again;
    }
}

template<typename T>
inline void
HeapMinBubbleDown(Heap<T>* s, u32 i)
{
    long smallest, left, right;
    Vec<T>& a = s->a;

again:
    left = HeapLeftI(i);
    right = HeapRightI(i);

    if (left < a.size && a[left] < a[i])
        smallest = left;
    else smallest = i;

    if (right < a.size && a[right] < a[smallest])
        smallest = right;

    if (smallest != i)
    {
        utils::swap(&a[i], &a[smallest]);
        i = smallest;
        goto again;
    }
}

template<typename T>
inline void
HeapMaxBubbleDown(Heap<T>* s, u32 i)
{
    long largest, left, right;
    Vec<T>& a = s->a;

again:
    left = HeapLeftI(i);
    right = HeapRightI(i);

    if (left < a.size && a[left] > a[i])
        largest = left;
    else largest = i;

    if (right < a.size && a[right] > a[largest])
        largest = right;

    if (largest != i)
    {
        utils::swap(&a[i], &a[largest]);
        i = largest;
        goto again;
    }
}

template<typename T>
inline void
HeapPushMin(Heap<T>* s, const T& x)
{
    VecPush(&s->a, x);
    HeapMinBubbleUp(s, s->a.size - 1);
}

template<typename T>
inline void
HeapPushMax(Heap<T>* s, const T& x)
{
    VecPush(&s->a, x);
    HeapMaxBubbleUp(s, s->a.size - 1);
}

template<typename T>
inline Heap<T>
HeapMinFromVec(Allocator* pA, const Vec<T>& a)
{
    Heap<T> q (pA, a.cap);
    q.a.size = a.size;
    utils::copy(q.a.pData, a.pData, a.size);

    for (long i = q.a.size / 2; i >= 0; i--)
        HeapMinBubbleDown(&q, i);

    return q;
}

template<typename T>
inline Heap<T>
HeapMaxFromVec(Allocator* pA, const Vec<T>& a)
{
    Heap<T> q (pA, a.cap);
    q.a.size = a.size;
    utils::copy(q.a.pData, a.pData, a.size);

    for (long i = q.a.size / 2; i >= 0; i--)
        HeapMaxBubbleDown(&q, i);

    return q;
}

template<typename T>
[[nodiscard]]
inline T
HeapMinExtract(Heap<T>* s)
{
    assert(s->a.size > 0 && "empty heap");

    utils::swap(&s->a[0], &s->a[s->a.size - 1]);
    T min = *VecPop(&s->a);
    HeapMinBubbleDown(s, 0);

    return min;
}

template<typename T>
[[nodiscard]]
inline T
HeapMaxExtract(Heap<T>* s)
{
    assert(s->a.size > 0 && "empty heap");

    utils::swap(&s->a[0], &s->a[s->a.size - 1]);
    T max = *VecPop(&s->a);
    HeapMaxBubbleDown(s, 0);

    return max;
}

template<typename T>
inline void
HeapMinSort(Allocator* pA, Vec<T>* a)
{
    Heap<T> s = HeapMinFromVec(pA, *a);

    for (u32 i = 0; i < a->size; i++)
        a->pData[i] = HeapMinExtract(&s);

    HeapDestroy(&s);
}

template<typename T>
inline void
HeapMaxSort(Allocator* pA, Vec<T>* a)
{
    Heap<T> s = HeapMaxFromVec(pA, *a);

    for (u32 i = 0; i < a->size; i++)
        a->pData[i] = HeapMaxExtract(&s);

    HeapDestroy(&s);
}

} /* namespace adt */

#pragma once

#include "utils.hh"

namespace adt
{

inline constexpr isize
HeapParentI(const isize i)
{
    return ((i + 1) / 2) - 1;
}

inline constexpr isize
HeapLeftI(const isize i)
{
    return ((i + 1) * 2) - 1;
}

inline constexpr isize
HeapRightI(const isize i)
{
    return HeapLeftI(i) + 1;
}

inline void
maxHeapify(auto* a, const isize size, isize i)
{
    isize largest, left, right;

GOTO_again:
    left = HeapLeftI(i);
    right = HeapRightI(i);

    if (left < size && a[left] > a[i])
        largest = left;
    else largest = i;

    if (right < size && a[right] > a[largest])
        largest = right;

    if (largest != (isize)i)
    {
        utils::swap(&a[i], &a[largest]);
        i = largest;
        goto GOTO_again;
    }
}

namespace sort
{

enum ORDER : u8 { INC, DEC };

inline constexpr bool
sorted(const auto* a, const isize size, const ORDER eOrder = INC)
{
    if (size <= 1) return true;

    if (eOrder == ORDER::INC)
    {
        for (isize i = 1; i < size; ++i)
            if (a[i - 1] > a[i]) return false;
    }
    else
    {
        for (isize i = size - 2; i >= 0; --i)
            if (a[i + 1] > a[i]) return false;
    }

    return true;
}

inline constexpr bool
sorted(const auto& a, const ORDER eOrder = INC)
{
    return sorted(a.data(), a.size(), eOrder);
}

template<typename T, isize (*FN_CMP)(const T&, const T&) = utils::compare<T>>
inline constexpr void
insertion(T* p, isize l, isize h)
{
    for (isize i = l + 1; i < h + 1; ++i)
    {
        T key = p[i];
        isize j = i;
        for (; j > l && FN_CMP(p[j - 1], key) > 0; --j)
            p[j] = p[j - 1];

        p[j] = key;
    }
}

template<template<typename> typename CON_T, typename T, isize (*FN_CMP)(const T&, const T&) = utils::compare<T>>
inline constexpr void
insertion(CON_T<T>* a)
{
    if (a->size() <= 1) return;

    insertion<T, FN_CMP>(a->data(), 0, a->size() - 1);
}

inline constexpr void
heapMax(auto* a, const isize size)
{
    isize heapSize = size;
    for (isize p = HeapParentI(heapSize); p >= 0; --p)
        maxHeapify(a, heapSize, p);

    for (isize i = size - 1; i > 0; --i)
    {
        utils::swap(&a[i], &a[0]);

        --heapSize;
        maxHeapify(a, heapSize, 0);
    }
}

inline constexpr auto
median3(const auto& x, const auto& y, const auto& z)
{
    if ((x < y && y < z) || (z < y && y < x)) return y;
    else if ((y < x && x < z) || (z < x && x < y)) return x;
    else return z;
}

template<typename T, isize (*FN_CMP)(const T&, const T&) = utils::compare<T>>
inline constexpr isize
partition(T a[], isize l, isize r, const T& pivot)
{
    while (l <= r)
    {
        while (FN_CMP(a[l], pivot) < 0) ++l;
        while (FN_CMP(a[r], pivot) > 0) --r;

        if (l <= r) utils::swap(&a[l++], &a[r--]);
    }

    return r;
}

template<typename T, isize (*FN_CMP)(const T&, const T&) = utils::compare<T>>
inline constexpr void
quick(T a[], isize l, isize r)
{
    if (l < r)
    {
        if ((r - l + 1) <= 64)
        {
            insertion<T, FN_CMP>(a, l, r);
            return;
        }

        T pivot = a[ median3(l, (l + r) / 2, r) ];
        isize i = l, j = r;

        while (i <= j)
        {
            while (FN_CMP(a[i], pivot) < 0) ++i;
            while (FN_CMP(a[j], pivot) > 0) --j;

            if (i <= j) utils::swap(&a[i++], &a[j--]);
        }

        if (l < j) quick<T, FN_CMP>(a, l, j);
        if (i < r) quick<T, FN_CMP>(a, i, r);
    }
}

template<template<typename> typename CON_T, typename T, isize (*FN_CMP)(const T&, const T&) = utils::compare<T>>
inline constexpr void
quick(CON_T<T>* pArrayContainer)
{
    if (pArrayContainer->size() <= 1) return;
    quick<T, FN_CMP>(pArrayContainer->data(), 0, pArrayContainer->size() - 1);
}

} /* namespace sort */
} /* namespace adt */

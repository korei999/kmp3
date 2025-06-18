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

template<typename CL_CMP>
inline constexpr void
insertion(auto* p, isize l, isize h, CL_CMP clCmp)
{
    for (isize i = l + 1; i < h + 1; ++i)
    {
        auto key = p[i];
        isize j = i;
        for (; j > l && clCmp(p[j - 1], key) > 0; --j)
            p[j] = p[j - 1];

        p[j] = key;
    }
}

template<typename T, typename CL_CMP> 
requires IsIndexable<T>
inline constexpr void
insertion(T* pArray, const CL_CMP clCmp)
{
    if (pArray->size() <= 1) return;
    insertion(pArray->data(), 0, pArray->size() - 1, clCmp);
}

template<typename T>
requires IsIndexable<T>
inline constexpr void
insertion(T* pArray)
{
    if (pArray->size() <= 1) return;
    insertion(pArray->data(), 0, pArray->size() - 1, utils::Comparator<decltype(pArray->operator[](0))> {});
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

template<typename T>
inline constexpr T
median3(const T& x, const T& y, const T& z)
{
    if ((x < y && y < z) || (z < y && y < x)) return y;
    else if ((y < x && x < z) || (z < x && x < y)) return x;
    else return z;
}

template<typename T, typename CL_CMP>
inline constexpr isize
partition(T a[], isize l, isize r, const T& pivot, CL_CMP clCmp)
{
    while (l <= r)
    {
        while (clCmp(a[l], pivot) < 0) ++l;
        while (clCmp(a[r], pivot) > 0) --r;

        if (l <= r) utils::swap(&a[l++], &a[r--]);
    }

    return r;
}

template<typename CL_CMP>
inline constexpr void
quick(auto a[], isize l, isize r, const CL_CMP clCmp)
{
    if (l < r)
    {
        if ((r - l + 1) <= 64)
        {
            insertion(a, l, r, clCmp);
            return;
        }

        auto pivot = a[ median3(l, (l + r) / 2, r) ];
        isize i = l, j = r;

        while (i <= j)
        {
            while (clCmp(a[i], pivot) < 0) ++i;
            while (clCmp(a[j], pivot) > 0) --j;

            if (i <= j) utils::swap(&a[i++], &a[j--]);
        }

        if (l < j) quick(a, l, j, clCmp);
        if (i < r) quick(a, i, r, clCmp);
    }
}

template<typename T, typename CL_CMP> 
requires IsIndexable<T>
inline constexpr void
quick(T* pArray, const CL_CMP clCmp)
{
    if (pArray->size() <= 1) return;
    quick(pArray->data(), 0, pArray->size() - 1, clCmp);
}

template<typename T>
requires IsIndexable<T>
inline constexpr void
quick(T* pArray)
{
    if (pArray->size() <= 1) return;
    quick(
        pArray->data(), 0, pArray->size() - 1,
        utils::Comparator<decltype(pArray->operator[](0))> {}
    );
}

template<typename ARRAY_T, typename T, ORDER ORDER>
inline isize
push(ARRAY_T* p, const T& x)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(p->data()[0])>, std::remove_cvref_t<T>>);

    ADT_ASSERT(p != nullptr, "");
    auto& a = *p;

    isize res = -1;

    if constexpr (ORDER == sort::ORDER::INC)
    {
        for (isize i = 0; i < a.size(); ++i)
        {
            if (utils::compare(x, a[i]) <= 0)
            {
                a.pushAt(i, x);
                res = i;
                break;
            }
        }
    }
    else
    {
        for (isize i = 0; i < a.size(); ++i)
        {
            if (utils::compare(x, a[i]) >= 0)
            {
                a.pushAt(i, x);
                res = i;
                break;
            }
        }
    }

    /* if failed to pushAt */
    if (res == -1) return a.push(x);

    return res;
}

template<typename ARRAY_T, typename T, ORDER ORDER>
inline isize
push(IAllocator* pAlloc, ARRAY_T* p, const T& x)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(p->data()[0])>, std::remove_cvref_t<T>>);

    ADT_ASSERT(p != nullptr, "");
    auto& a = *p;

    isize res = -1;

    if constexpr (ORDER == sort::ORDER::INC)
    {
        for (isize i = 0; i < a.size(); ++i)
        {
            if (utils::compare(x, a[i]) <= 0)
            {
                a.pushAt(pAlloc, i, x);
                res = i;
                break;
            }
        }
    }
    else
    {
        for (isize i = 0; i < a.size(); ++i)
        {
            if (utils::compare(x, a[i]) >= 0)
            {
                a.pushAt(pAlloc, i, x);
                res = i;
                break;
            }
        }
    }

    /* if failed to pushAt */
    if (res == -1) return a.push(pAlloc, x);

    return res;
}

template<typename ARRAY_T, typename T>
inline isize
push(const ORDER eOrder, ARRAY_T* p, const T& x)
{
    if (eOrder == ORDER::INC)
        return push<ARRAY_T, T, ORDER::INC>(p, x);
    else return push<ARRAY_T, T, ORDER::DEC>(p, x);
}

template<typename ARRAY_T, typename T>
inline isize
push(IAllocator* pAlloc, const ORDER eOrder, ARRAY_T* p, const T& x)
{
    if (eOrder == ORDER::INC)
        return push<ARRAY_T, T, ORDER::INC>(pAlloc, p, x);
    else return push<ARRAY_T, T, ORDER::DEC>(pAlloc, p, x);
}

} /* namespace sort */
} /* namespace adt */

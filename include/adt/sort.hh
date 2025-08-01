#pragma once

#include "IAllocator.hh"
#include "Thread.hh"
#include "defer.hh"
#include "utils.hh"

namespace adt
{

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
        if ((r - l + 1) <= 32)
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

        quick(a, l, j, clCmp);
        quick(a, i, r, clCmp);
    }
}

template<typename THREAD_POOL_T, typename CL_CMP>
inline void
quickParallel(
    THREAD_POOL_T* pTPool,
    auto a[],
    isize l,
    isize r,
    CL_CMP clCmp
)
{
    if (l < r)
    {
        if ((r - l + 1) <= 32)
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

        Future<Empty> fut {INIT};
        ADT_DEFER( fut.destroy() );
        bool bSpawned = false;

        auto clDo = [&]
        {
            quickParallel(pTPool, a, l, j, clCmp);
            fut.signal();

            return 0;
        };

        if ((j - l + 1) <= SIZE_8K)
        {
            quick(a, l, j, clCmp);
        }
        else
        {
            pTPool->addLambdaRetryOrDo(clDo);
            bSpawned = true;
        }

        quickParallel(pTPool, a, i, r, clCmp);

        if (bSpawned) fut.wait();
    }
}

template<typename THREAD_POOL_T, typename T>
requires IsIndexable<T>
inline void
quickParallel(THREAD_POOL_T* pThreadPool, T* pArray)
{
    if (pArray->size() <= 1) return;
    quickParallel(
        pThreadPool,
        pArray->data(), 0, pArray->size() - 1,
        utils::Comparator<decltype(pArray->operator[](0))> {}
    );
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

template<ORDER ORDER, typename ARRAY_T, typename T>
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

template<ORDER ORDER, typename ARRAY_T, typename T>
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
        return push<ORDER::INC, ARRAY_T, T>(p, x);
    else return push<ORDER::INC, ARRAY_T, T>(p, x);
}

template<typename ARRAY_T, typename T>
inline isize
push(IAllocator* pAlloc, const ORDER eOrder, ARRAY_T* p, const T& x)
{
    if (eOrder == ORDER::INC)
        return push<ORDER::INC, ARRAY_T, T>(pAlloc, p, x);
    else return push<ORDER::INC, ARRAY_T, T>(pAlloc, p, x);
}

} /* namespace sort */
} /* namespace adt */

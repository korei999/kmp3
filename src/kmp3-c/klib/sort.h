#pragma once

#include "Ctx.h"

static inline ssize_t k_sort_median3(ssize_t x, ssize_t y, ssize_t z);
static inline bool k_sort_quick(
    k_Span sp,
    ssize_t memberSize,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
); /* Can fail if allocator fails. */
static inline bool k_sort_quick2(
    void* pData,
    ssize_t memberSize,
    ssize_t l,
    ssize_t r,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
);
static inline void k_sort_insertion(
    k_Span sp,
    ssize_t memberSize,
    void* pSwap, /* Of memberSize size (temp variable placeholder). */
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
);
static inline void k_sort_insertion2(
    void* pData,
    ssize_t memberSize,
    void* pSwap, /* Of memberSize size. (temp variable placeholder). */
    ssize_t l,
    ssize_t r,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
);

static inline ssize_t
k_sort_median3(ssize_t x, ssize_t y, ssize_t z)
{
    if ((x < y && y < z) || (z < y && y < x)) return y;
    else if ((y < x && x < z) || (z < x && x < y)) return x;
    else return z;
}

static inline bool
k_sort_quick(
    k_Span sp,
    ssize_t memberSize,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
)
{
    if (sp.size <= 1) return true;
    return k_sort_quick2(sp.pData, memberSize, 0, sp.size - 1, pfnCmp, pArg);
}

static inline bool
k_sort_quick2(
    void* pData,
    ssize_t memberSize,
    ssize_t l,
    ssize_t r,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
)
{
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    uint8_t* p = pData;
    const ssize_t size = (r - l + 1);
    ssize_t* pStack = k_ArenaMalloc(pArena, sizeof(ssize_t)*size + memberSize*2);
    if K_UNLIKELY(!pStack)
    {
        /* No need to restore the arena. */
        return false;
    }

    void* pPivot = (uint8_t*)pStack + sizeof(ssize_t) * size;
    void* pSwap = (uint8_t*)pPivot + memberSize;

    ssize_t stackI = 0;
    pStack[stackI++] = l;
    pStack[stackI++] = r;

    while (stackI > 0)
    {
        r = pStack[--stackI];
        l = pStack[--stackI];

        if (l < r)
        {
            if ((r - l + 1) <= 32)
            {
                /* Much faster for small arrays. */
                k_sort_insertion2(p, memberSize, pSwap, l, r, pfnCmp, pArg);
            }
            else
            {
                ssize_t pivotI = k_sort_median3(l, (l + r) / 2, r);
                memcpy(pPivot, p + pivotI*memberSize, memberSize);
                ssize_t i = l, j = r;

                while (i <= j)
                {
                    while (pfnCmp(p + i*memberSize, pPivot, pArg) < 0) ++i;
                    while (pfnCmp(p + j*memberSize, pPivot, pArg) > 0) --j;

                    if (i <= j)
                    {
                        memcpy(pSwap, p + i*memberSize, memberSize);
                        memcpy(p + i*memberSize, p + j*memberSize, memberSize);
                        memcpy(p + j*memberSize, pSwap, memberSize);
                        ++i, --j;
                    }
                }

                pStack[stackI++] = l;
                pStack[stackI++] = j;

                pStack[stackI++] = i;
                pStack[stackI++] = r;
            }
        } /* if (l < r) */
    } /* while (stackI > 0) */

    k_ArenaStateRestore(&arenaState);
    return true;
}

static inline void
k_sort_insertion(
    k_Span sp,
    ssize_t memberSize,
    void* pSwap,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
)
{
    if (sp.size <= 1) return;
    k_sort_insertion2(sp.pData, memberSize, pSwap, 0, sp.size - 1, pfnCmp, pArg);
}

static inline void
k_sort_insertion2(
    void* pData,
    ssize_t memberSize,
    void* pSwap,
    ssize_t l,
    ssize_t r,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
)
{
    uint8_t* p = pData;

    for (ssize_t i = l + 1; i < r + 1; ++i)
    {
        memcpy(pSwap, p + i*memberSize, memberSize);
        ssize_t j = i;
        for (; j > l && pfnCmp(p + (j - 1)*memberSize, pSwap, pArg) > 0; --j)
            memcpy(p + j*memberSize, p + (j - 1)*memberSize, memberSize);

        memcpy(p + j*memberSize, pSwap, memberSize);
    }
}

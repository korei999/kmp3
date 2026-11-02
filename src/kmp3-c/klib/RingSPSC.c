#include "RingSPSC.h"

bool
k_RingSPSCInit(k_RingSPSC* s, k_IAllocator* pAlloc, ssize_t cap)
{
    *s = (k_RingSPSC){0};

    cap = k_nextPowerofTwo64(cap);
    if (cap <= 0) return false;

    void* pNew = k_IAllocatorZalloc(pAlloc, cap);
    if (!pNew) return false;

    s->pData = pNew;
    s->capMinusOne = cap - 1;

    return true;
}

void
k_RingSPSCDestroy(k_RingSPSC* s, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, s->pData);
}

bool
k_RingSPSCPush(k_RingSPSC* s, const uint8_t* p, ssize_t size, ssize_t* pSizeOrNull)
{
    const uint64_t tailI = k_atomic_U64LoadRelaxed(&s->atomTailI);
    const uint64_t headI = k_atomic_U64LoadAcquire(&s->atomHeadI);
    const uint64_t availableSize = s->capMinusOne - (tailI - headI); /* Leave 1 to avoid overlapping tail with head on full queue. */

    if (availableSize < size)
    {
        if (pSizeOrNull) *pSizeOrNull = availableSize;
        return false;
    }

    const uint64_t realTailI = tailI & s->capMinusOne;
    const uint64_t nUntilEnd = K_MIN((s->capMinusOne + 1) - realTailI, size);
    memcpy(s->pData + realTailI, p, nUntilEnd);
    memcpy(s->pData, p + nUntilEnd, size - nUntilEnd);

    k_atomic_U64AddRelease(&s->atomTailI, size);

    if (pSizeOrNull) *pSizeOrNull = availableSize - size;

    return true;
}

ssize_t
k_RingSPSCPop(k_RingSPSC* s, uint8_t* pFill, ssize_t fillBuffSize)
{
    const uint64_t tailI = k_atomic_U64LoadRelaxed(&s->atomTailI);
    const uint64_t headI = k_atomic_U64LoadAcquire(&s->atomHeadI);
    const uint64_t ringSize = tailI - headI;

    if (ringSize == 0) return 0;

    fillBuffSize = K_MIN(fillBuffSize, ringSize);

    const uint64_t realHeadI = headI & s->capMinusOne;
    const uint64_t nUntilEnd = K_MIN((s->capMinusOne + 1) - realHeadI, fillBuffSize);
    memcpy(pFill, s->pData + realHeadI, nUntilEnd);
    memcpy(pFill + nUntilEnd, s->pData, fillBuffSize - nUntilEnd);

    k_atomic_U64AddRelease(&s->atomHeadI, fillBuffSize);

    return fillBuffSize;
}


ssize_t
k_RingSPSCSize(k_RingSPSC* s)
{
    return k_atomic_U64LoadRelaxed(&s->atomTailI) - k_atomic_U64LoadRelaxed(&s->atomHeadI);
}

ssize_t
k_RingSPSCCap(k_RingSPSC* s)
{
    return s->capMinusOne + 1;
}

void
k_RingSPSCClear(k_RingSPSC* s)
{
    const uint64_t tailI = k_atomic_U64LoadRelaxed(&s->atomTailI);
    const uint64_t headI = k_atomic_U64LoadAcquire(&s->atomHeadI);
    const uint64_t ringSize = tailI - headI;

    if (ringSize == 0) return;

    const uint64_t realHeadI = headI & s->capMinusOne;
    const uint64_t nUntilEnd = K_MIN((s->capMinusOne + 1) - realHeadI, ringSize);
    memset(s->pData + realHeadI, 0, nUntilEnd);
    memset(s->pData, 0, ringSize - nUntilEnd);

    k_atomic_U64AddRelease(&s->atomHeadI, ringSize);
}

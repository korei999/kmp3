#include "RingMPSC.h"
#include "Thread.h"

static const K_RING_MPSC_SIZE_T MAX_PUSH_SIZE = (K_RING_MPSC_SIZE_T)-1;

#pragma pack(1)
typedef struct Header
{
    K_RING_MPSC_SIZE_T size;
} Header;
#pragma pack()

bool
k_RingMPSCInit(k_RingMPSC* s, k_IAllocator* pAlloc, size_t capPo2)
{
    const size_t cap = k_nextPowerofTwo64(capPo2);
    void* pNew = k_IAllocatorZalloc(pAlloc, cap);
    if K_UNLIKELY(!pNew) return false;

    s->headI.volNum = 0;
    s->tailI.volNum = 0;
    s->pData = pNew;
    s->capMinus1 = cap - 1;

    k_atomic_U64StoreRelease(&s->pushTailI, 0);

    return true;
}

void
k_RingMPSCDestroy(k_RingMPSC* s, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, s->pData);
    *s = (k_RingMPSC){0};
}

static void
k_RingMPSCPushUnsafe(k_RingMPSC* s, uint64_t tailI, const void* p, size_t size)
{
    tailI &= s->capMinus1;
    const ssize_t tailToEnd = K_MIN(s->capMinus1 + 1 - tailI, size);
    memcpy(s->pData + tailI, p, tailToEnd);
    memcpy(s->pData, (uint8_t*)p + tailToEnd, size - tailToEnd);
}

static void
k_RingMPSCPopUnsafe(k_RingMPSC* s, uint64_t headI, void* p, size_t size)
{
    headI &= s->capMinus1;
    const ssize_t headToEnd = K_MIN(s->capMinus1 + 1 - headI, size);
    memcpy(p, s->pData + headI, headToEnd);
    memcpy((uint8_t*)p + headToEnd, s->pData, size - headToEnd);
}

bool
k_RingMPSCPushV(k_RingMPSC* s, const k_Span* pSps, ssize_t nSpans)
{
    K_RING_MPSC_SIZE_T totalSize;
    {
        uint64_t totalSize2 = 0;
        for (ssize_t i = 0; i < nSpans; ++i) totalSize2 += pSps[i].size;
        assert(totalSize2 <= MAX_PUSH_SIZE);
        if (totalSize2 > MAX_PUSH_SIZE) return false;
        totalSize = totalSize2;
    }

    uint64_t headI;
    uint64_t pushTailI = k_atomic_U64LoadRelaxed(&s->pushTailI);

again:
    headI = k_atomic_U64LoadRelaxed(&s->headI);
    if (((pushTailI - headI) + totalSize + sizeof(Header)) > s->capMinus1 + 1)
        return false;

    if (k_atomic_U64CASRelaxed(&s->pushTailI, &pushTailI, pushTailI + totalSize + sizeof(Header)))
    {
        k_RingMPSCPushUnsafe(s, pushTailI, &totalSize, sizeof(K_RING_MPSC_SIZE_T));

        for (ssize_t off = 0, i = 0; i < nSpans; off += pSps[i].size, ++i)
            k_RingMPSCPushUnsafe(s, pushTailI + sizeof(Header) + off, pSps[i].pData, pSps[i].size);

        uint64_t ourTailI = pushTailI;
        while (!k_atomic_U64CASRelease(&s->tailI, &ourTailI, pushTailI + totalSize + sizeof(Header)))
        {
            ourTailI = pushTailI;
            k_ThreadYield();
        }
    }
    else
    {
        goto again;
    }

    return true;
}

k_Span
k_RingMPSCPop(k_RingMPSC* s, k_RingMPSCPopOpts opts)
{
    uint64_t headI = k_atomic_U64LoadRelaxed(&s->headI);
    if (k_atomic_U64LoadAcquire(&s->tailI) == headI) return (k_Span){0};

    K_RING_MPSC_SIZE_T headerSize;
    k_RingMPSCPopUnsafe(s, headI, &headerSize, sizeof(Header));

    if (!opts.pDestOrNull || opts.destSize < headerSize)
    {
        assert(opts.pAlloc);
        opts.pDestOrNull = k_IAllocatorMalloc(opts.pAlloc, headerSize);
    }

    k_RingMPSCPopUnsafe(s, headI + sizeof(Header), opts.pDestOrNull, headerSize);
    k_atomic_U64AddRelease(&s->headI, sizeof(Header) + headerSize);

    return (k_Span){opts.pDestOrNull, headerSize};
}

ssize_t
k_RingMPSCHeaderSize(void)
{
    return sizeof(Header);
}

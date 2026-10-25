#include "RingBuffer.h"

bool
k_RingBufferInit(k_RingBuffer* s, k_IAllocator* pAlloc, ssize_t cap)
{
    ssize_t capPo2 = k_isPowerOf2(cap) ? cap : k_nextPowerofTwo64(cap);
    uint8_t* pNewData = k_IAllocatorMalloc(pAlloc, capPo2);
    if (!pNewData) return false;

    assert(k_isPowerOf2(capPo2));

    s->priv.pData = pNewData;
    s->priv.headI = 0;
    s->priv.tailI = 0;
    s->priv.size = 0;
    s->priv.capMinus1 = capPo2 - 1;

    return true;
}

void
k_RingBufferDestroy(k_RingBuffer* s, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, s->priv.pData);
    *s = (k_RingBuffer){0};
}

void
k_RingBufferPushNoChecks(k_RingBuffer* pSelf, const void* p, ssize_t size)
{
    K_TYPEOF(pSelf->priv)* s = &pSelf->priv;
    const ssize_t nextTailI = (s->tailI + size) & (s->capMinus1);

    const ssize_t tailToEnd = K_MIN(s->capMinus1 + 1 - s->tailI, size);
    memcpy(s->pData + s->tailI, p, tailToEnd);
    memcpy(s->pData, (uint8_t*)p + tailToEnd, size - tailToEnd);

    s->size += size;
    s->tailI = nextTailI;
}

void
k_RingBufferPopNoChecks(k_RingBuffer* pSelf, void* p, ssize_t size)
{
    K_TYPEOF(pSelf->priv)* s = &pSelf->priv;
    const ssize_t nextHeadI = (s->headI + size) & (s->capMinus1);

    const ssize_t headToEnd = K_MIN(s->capMinus1 + 1 - s->headI, size);
    memcpy(p, s->pData + s->headI, headToEnd);
    memcpy((uint8_t*)p + headToEnd, s->pData, size - headToEnd);

    s->size -= size;
    s->headI = nextHeadI;
}

bool
k_RingBufferPush(k_RingBuffer* s, const void* p, ssize_t size)
{
    if (size <= 0) return true;
    if (size + s->priv.size > k_RingBufferCap(s)) return false;
    k_RingBufferPushNoChecks(s, p, size);
    return true;
}

bool
k_RingBufferPushV(k_RingBuffer* s, const k_Span* pSps, ssize_t spCount)
{
    ssize_t totalSize = 0;
    for (ssize_t i = 0; i < spCount; ++i) totalSize += pSps[i].size;
    if (totalSize <= 0) return true;
    if (totalSize + s->priv.size >= k_RingBufferCap(s)) return false;

    for (ssize_t i = 0; i < spCount; ++i)
        k_RingBufferPushNoChecks(s, pSps[i].pData, pSps[i].size);

    return true;
}

bool
k_RingBufferPop(k_RingBuffer* s, void* p, ssize_t size)
{
    if (size <= 0) return true;
    if (size > s->priv.size) return false;
    k_RingBufferPopNoChecks(s, p, size);
    return true;
}

bool
k_RingBufferPopV(k_RingBuffer* s, k_Span* pSps, ssize_t spCount)
{
    ssize_t totalSize = 0;
    for (ssize_t i = 0; i < spCount; ++i) totalSize += pSps[i].size;
    if (totalSize <= 0) return true;
    if (totalSize > s->priv.size) return false;

    for (ssize_t i = 0; i < spCount; ++i)
        k_RingBufferPopNoChecks(s, pSps[i].pData, pSps[i].size);

    return true;
}

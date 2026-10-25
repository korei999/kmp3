/*  Multi-producer/multi-consumer bounded queue.
 *  Copyright (c) 2010-2011, Dmitry Vyukov. All rights reserved.
 *  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright notice, this list of
 *        conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright notice, this list
 *        of conditions and the following disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *  THIS SOFTWARE IS PROVIDED BY DMITRY VYUKOV "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *  DMITRY VYUKOV OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  The views and conclusions contained in the software and documentation are those of the authors and should not be interpreted
 *  as representing official policies, either expressed or implied, of Dmitry Vyukov. */

#pragma once

#include "IAllocator.h"
#include "Span.h"
#include "atomic.h"

typedef struct k_QueueMPMCSlot
{
    k_atomic_U64 seq;
    uint8_t pMem[];
} k_QueueMPMCSlot;

typedef struct k_QueueMPMC
{
    k_atomic_U64 headI;
    char aPad0[56];
    k_atomic_U64 tailI;
    char aPad1[56];
    uint8_t* pData;
    int capMinusOne;
    int stride;
} k_QueueMPMC;

typedef struct k_QueueMPMCInitOpts
{
    int slotSize; /* Aligned to sizeof(k_QueueMPMCSlot). */
    int cap; /* Rounded to the next power of two. */
} k_QueueMPMCInitOpts;

static inline bool k_QueueMPMCInit(k_QueueMPMC* s, k_IAllocator* pAlloc, k_QueueMPMCInitOpts opts);
static inline void k_QueueMPMCDestroy(k_QueueMPMC* s, k_IAllocator* pAlloc);
static inline bool k_QueueMPMCPush(k_QueueMPMC* s, const void* p, ssize_t size);
static inline bool k_QueueMPMCPushV(k_QueueMPMC* s, const k_Span* pSpans, ssize_t nSpans);
static inline bool k_QueueMPMCPop(k_QueueMPMC* s, void* pDest, ssize_t size);
static inline ssize_t k_QueueMPMCSlotSize(const k_QueueMPMC* s);
static inline ssize_t k_QueueMPMCSize(const k_QueueMPMC* s);
static inline ssize_t k_QueueMPMCCap(const k_QueueMPMC* s);

static inline bool
k_QueueMPMCInit(k_QueueMPMC* s, k_IAllocator* pAlloc, k_QueueMPMCInitOpts opts)
{
    const ssize_t cap = k_nextPowerofTwo64(opts.cap);
    const ssize_t slotSize = K_ALIGN_UP(opts.slotSize, _Alignof(k_QueueMPMCSlot));
    const ssize_t totalCap = (sizeof(k_QueueMPMCSlot) + slotSize) * cap;
    uint8_t* pNew = k_IAllocatorZalloc(pAlloc, totalCap);
    if (!pNew) return false;

    s->headI.volNum = 0;
    s->pData = pNew;
    s->capMinusOne = cap - 1;
    s->stride = sizeof(k_QueueMPMCSlot) + slotSize;

    for (ssize_t i = 0; i < cap; ++i)
    {
        k_QueueMPMCSlot* pSlot = (k_QueueMPMCSlot*)(s->pData + s->stride*i);
        k_atomic_U64StoreRelaxed(&pSlot->seq, i);
    }

    k_atomic_U64StoreRelease(&s->tailI, 0);

    return true;
}

static inline bool
k_QueueMPMCPush(k_QueueMPMC* s, const void* p, ssize_t size)
{
    assert(size <= s->stride - (ssize_t)sizeof(k_QueueMPMCSlot));

    uint64_t tailI = k_atomic_U64LoadRelaxed(&s->tailI);
    k_QueueMPMCSlot* pSlot;

again:
    pSlot = (k_QueueMPMCSlot*)(s->pData + (tailI & s->capMinusOne)*s->stride);
    uint64_t seq = k_atomic_U64LoadAcquire(&pSlot->seq);
    ssize_t diff = (ssize_t)seq - (ssize_t)tailI;
    if (diff == 0)
    {
        if (!k_atomic_U64CASRelaxed(&s->tailI, &tailI, tailI + 1))
            goto again;
    }
    else if (diff < 0)
    {
        return false;
    }
    else
    {
        tailI = k_atomic_U64LoadRelaxed(&s->tailI);
        goto again;
    }

    memcpy(pSlot->pMem, p, size);
    k_atomic_U64StoreRelease(&pSlot->seq, tailI + 1);
    return true;
}

static inline bool
k_QueueMPMCPushV(k_QueueMPMC* s, const k_Span* pSpans, ssize_t nSpans)
{
    ssize_t totalSize = 0;
    for (ssize_t i = 0; i < nSpans; ++i) totalSize += pSpans[i].size;
    assert(totalSize <= s->stride - (ssize_t)sizeof(k_QueueMPMCSlot));
    (void)totalSize;

    uint64_t tailI = k_atomic_U64LoadRelaxed(&s->tailI);
    k_QueueMPMCSlot* pSlot;

again:
    pSlot = (k_QueueMPMCSlot*)(s->pData + (tailI & s->capMinusOne)*s->stride);
    uint64_t seq = k_atomic_U64LoadAcquire(&pSlot->seq);
    ssize_t diff = (ssize_t)seq - (ssize_t)tailI;
    if (diff == 0)
    {
        if (!k_atomic_U64CASRelaxed(&s->tailI, &tailI, tailI + 1))
            goto again;
    }
    else if (diff < 0)
    {
        return false;
    }
    else
    {
        tailI = k_atomic_U64LoadRelaxed(&s->tailI);
        goto again;
    }

    for (ssize_t i = 0, off = 0; i < nSpans; off += pSpans[i].size, ++i)
        memcpy(pSlot->pMem + off, pSpans[i].pData, pSpans[i].size);
    k_atomic_U64StoreRelease(&pSlot->seq, tailI + 1);
    return true;
}

static inline bool
k_QueueMPMCPop(k_QueueMPMC* s, void* pDest, ssize_t size)
{
    assert(size <= s->stride - (ssize_t)sizeof(k_QueueMPMCSlot));

    uint64_t headI = k_atomic_U64LoadRelaxed(&s->headI);
    k_QueueMPMCSlot* pSlot;

again:
    pSlot = (k_QueueMPMCSlot*)(s->pData + (headI & s->capMinusOne)*s->stride);
    uint64_t seq = k_atomic_U64LoadRelaxed(&pSlot->seq);
    ssize_t diff = (ssize_t)seq - (ssize_t)(headI + 1);
    if (diff == 0)
    {
        if (!k_atomic_U64CASRelaxed(&s->headI, &headI, headI + 1))
            goto again;
    }
    else if (diff < 0)
    {
        return false;
    }
    else
    {
        headI = k_atomic_U64LoadRelaxed(&s->headI);
        goto again;
    }

    memcpy(pDest, pSlot->pMem, size);
    k_atomic_U64StoreRelease(&pSlot->seq, s->capMinusOne + 1 + headI);
    return true;
}

static inline ssize_t
k_QueueMPMCSlotSize(const k_QueueMPMC* s)
{
    assert((s->stride - (ssize_t)sizeof(k_QueueMPMCSlot)) >= 0);
    return s->stride - (ssize_t)sizeof(k_QueueMPMCSlot);
}

static inline ssize_t
k_QueueMPMCSize(const k_QueueMPMC* s)
{
    return k_atomic_U64LoadRelaxed(&s->tailI) - k_atomic_U64LoadRelaxed(&s->headI);
}

static inline ssize_t
k_QueueMPMCCap(const k_QueueMPMC* s)
{
    return s->capMinusOne + 1;
}

static inline void
k_QueueMPMCDestroy(k_QueueMPMC* s, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, s->pData);
    *s = (k_QueueMPMC){0};
}

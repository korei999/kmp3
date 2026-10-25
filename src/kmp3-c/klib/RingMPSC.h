#pragma once

#include "IAllocator.h"
#include "atomic.h"
#include "Span.h"

typedef uint16_t K_RING_MPSC_SIZE_T;

/* Lock free(ish). */
typedef struct k_RingMPSC
{
    k_atomic_U64 headI;
    char aPad0[64];
    k_atomic_U64 tailI;
    char aPad1[64];
    k_atomic_U64 pushTailI;
    char aPad2[64];
    uint8_t* pData;
    size_t capMinus1;
} k_RingMPSC;

typedef struct k_RingMPSCPopOpts
{
    void* pDestOrNull; /* If null use pAlloc to allocate the buffer. */
    K_RING_MPSC_SIZE_T destSize;
    k_IAllocator* pAlloc; /* Use if payload size is greater than destSize. */
} k_RingMPSCPopOpts;

bool k_RingMPSCInit(k_RingMPSC* s, k_IAllocator* pAlloc, size_t capPo2);
void k_RingMPSCDestroy(k_RingMPSC* s, k_IAllocator* pAlloc);
static inline bool k_RingMPSCPush(k_RingMPSC* s, const void* pData, K_RING_MPSC_SIZE_T size);
bool k_RingMPSCPushV(k_RingMPSC* s, const k_Span* pSps, ssize_t nSpans);
k_Span k_RingMPSCPop(k_RingMPSC* s, k_RingMPSCPopOpts opts);
static inline ssize_t k_RingMPSCCap(const k_RingMPSC* s);
ssize_t k_RingMPSCHeaderSize(void);

static inline bool
k_RingMPSCPush(k_RingMPSC* s, const void* pData, K_RING_MPSC_SIZE_T size)
{
    const k_Span sp = {.pData = (void*)pData, .size = size};
    return k_RingMPSCPushV(s, &sp, 1);
}

static inline ssize_t
k_RingMPSCCap(const k_RingMPSC* s)
{
    return (ssize_t)s->capMinus1;
}

#pragma once

#include "IAllocator.h"
#include "Span.h"
#include "StringView.h"

/* Fixed capacity circular array. */
typedef struct k_RingBuffer
{
    struct
    {
        uint8_t* pData;
        ssize_t headI;
        ssize_t tailI;
        ssize_t size;
        ssize_t capMinus1;
    } priv;
} k_RingBuffer;

bool k_RingBufferInit(k_RingBuffer* s, k_IAllocator* pAlloc, ssize_t cap);
void k_RingBufferDestroy(k_RingBuffer* s, k_IAllocator* pAlloc);
bool k_RingBufferPush(k_RingBuffer* s, const void* p, ssize_t size);
bool k_RingBufferPushV(k_RingBuffer* s, const k_Span* pSps, ssize_t spCount);
bool k_RingBufferPop(k_RingBuffer* s, void* p, ssize_t size); /* memcpy into p. */
bool k_RingBufferPopV(k_RingBuffer* s, k_Span* pSps, ssize_t spCount);
void k_RingBufferPushNoChecks(k_RingBuffer* s, const void* p, ssize_t size); /* NOTE: Does not perform any range checking. */
void k_RingBufferPopNoChecks(k_RingBuffer* pSelf, void* p, ssize_t size); /* NOTE: Does not perform any range checking. */
static inline ssize_t k_RingBufferSize(k_RingBuffer* s);
static inline ssize_t k_RingBufferCap(k_RingBuffer* s);
static inline bool k_RingBufferEmpty(k_RingBuffer* s);

static inline ssize_t
k_RingBufferSize(k_RingBuffer* s)
{
    return s->priv.size;
}

static inline ssize_t
k_RingBufferCap(k_RingBuffer* s)
{
    return s->priv.capMinus1;
}

static inline bool
k_RingBufferPushSv(k_RingBuffer* s, const k_StringView sv)
{
    return k_RingBufferPush(s, (uint8_t*)sv.pData, sv.size);
}

static inline bool
k_RingBufferEmpty(k_RingBuffer* s)
{
    return s->priv.size <= 0;
}

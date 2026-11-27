#pragma once

#include "common.h"

#include <assert.h>
#include <string.h>

typedef struct
{
    void* pData;
    ssize_t size;
} k_Span;

#define K_SPAN(pData, size) (k_Span){(uint8_t*)pData, (ssize_t)(sizeof(*pData) * size)}
#define K_SPAN_GET(sp, Type, idx) ((void)assert(idx >= 0 && idx*(ssize_t)sizeof(Type) < (sp).size), (Type*)((uint8_t*)(sp).pData + (ssize_t)sizeof(Type)*idx))
#define K_SPAN_SET(sp, Type, idx, val) ((void)assert(idx >= 0 && idx*(ssize_t)sizeof(Type) < (sp).size), *(Type*)((uint8_t*)(sp).pData + (ssize_t)sizeof(Type)*idx) = val)

typedef struct
{
    int width;
    int height;
    int stride;
} k_Span2D;

static inline void*
k_Span2DGet(k_Span2D sp, void* pSpanData, ssize_t memberSize, ssize_t x, ssize_t y)
{
    assert(x >= 0 && x < sp.width && y >= 0 && y < sp.height);
    return (uint8_t*)pSpanData + (sp.width*memberSize*y) + x*memberSize;
}

static inline void
k_Span2DSet(k_Span2D sp, void* pSpanData, ssize_t memberSize, ssize_t x, ssize_t y, void* pWriteData)
{
    memcpy(k_Span2DGet(sp, pSpanData, memberSize, x, y), pWriteData, memberSize);
}

#define K_SPAN_2D_GET(sp, data, x, y) ((K_TYPEOF(data))k_Span2DGet(sp, data, sizeof(data[0]), x, y))
#define K_SPAN_2D_SET(sp, data, x, y, ...) k_Span2DSet(sp, data, sizeof(data[0]), x, y, __VA_ARGS__)

/* Usage example:

typdef struct
{
    union
    {
        k_Span2D;
        k_Span2D base;
    };
    int* pData;
} Span2DDerived;

Span2DDerived sp = {.pData = pData, .width = width, .height = height};
int* p12 = K_SPAN_2D_GET(sp.base, sp.pData, 1, 2);
K_SPAN_2D_SET(sp.base, sp.pData, 1, 3, &(int){777});

*/

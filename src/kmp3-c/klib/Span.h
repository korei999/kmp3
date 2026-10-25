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
    void* pData;
    int width;
    int height;
    int stride;
    int memberSize;
} k_Span2D;

static inline void*
k_Span2DGet(k_Span2D sp, ssize_t x, ssize_t y)
{
    assert(x >= 0 && x < sp.width && y >= 0 && y < sp.height);
    return (uint8_t*)sp.pData + (sp.width*sp.memberSize*y) + x*sp.memberSize;
}

static inline void
k_Span2DSet(k_Span2D sp, ssize_t x, ssize_t y, void* pData)
{
    memcpy(k_Span2DGet(sp, x, y), pData, sp.memberSize);
}

#define K_SPAN2D_SET(sp, Type, x, y, val) ((void)assert(x >= 0 && x < (sp).width && y >= 0 && y < (sp).height && (int)sizeof(Type) == (sp).memberSize), *(Type*)((uint8_t*)(sp).pData + ((int)sizeof(Type)*(sp).width*y) + (int)sizeof(Type)*x) = val)

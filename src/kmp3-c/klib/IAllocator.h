#pragma once

#include "common.h"

#include <string.h>

#ifdef K_ASAN
    #include "sanitizer/asan_interface.h"
    #define K_ASAN_POISON ASAN_POISON_MEMORY_REGION
    #define K_ASAN_UNPOISON ASAN_UNPOISON_MEMORY_REGION
#else
    #define K_ASAN_POISON(...) (void)0
    #define K_ASAN_UNPOISON(...) (void)0
#endif

typedef struct k_IAllocator k_IAllocator;
typedef struct k_IAllocatorVTable
{
    void* (*malloc)(k_IAllocator* pSelf, ssize_t nBytes);
    void* (*zalloc)(k_IAllocator* pSelf, ssize_t nBytes);
    void* (*realloc)(k_IAllocator* pSelf, void* p, ssize_t nBytesOld, ssize_t nBytesNew);
    void (*free)(k_IAllocator* pSelf, void* p);
} k_IAllocatorVTable;

typedef struct k_IAllocator
{
    const k_IAllocatorVTable* pVTable;
} k_IAllocator;

K_NO_DISCARD static inline void*
k_IAllocatorMalloc(k_IAllocator* s, ssize_t nBytes)
{
    return s->pVTable->malloc(s, nBytes);
}

K_NO_DISCARD static inline void*
k_IAllocatorZalloc(k_IAllocator* s, ssize_t nBytes)
{
    return s->pVTable->zalloc(s, nBytes);
}

K_NO_DISCARD static inline void*
k_IAllocatorRealloc(k_IAllocator* s, void* p, ssize_t nBytesOld, ssize_t nBytesNew)
{
    return s->pVTable->realloc(s, p, nBytesOld, nBytesNew);
}

static inline void
k_IAllocatorFree(k_IAllocator* s, void* p)
{
    s->pVTable->free(s, p);
}

K_NO_DISCARD static inline void*
k_IAllocatorAlloc(k_IAllocator* s, void* p, ssize_t size)
{
    void* ret = s->pVTable->malloc(s, size);
    if K_UNLIKELY(!ret) return NULL;
    memcpy(ret, p, size);
    return ret;
}

#define K_ALIGN_UP_PO2(x, to) ((x) + to - 1) & (~(to - 1))
#define K_ALIGN_DOWN_PO2(x, to) (x & ~(to - 1))
#define K_ALIGN_UP8(x) K_ALIGN_UP_PO2(x, 8)
#define K_ALIGN_DOWN8(x) K_ALIGN_DOWN_PO2(x, 8)

#define K_ALIGN_UP(x, to) ((((x) + (to) - 1) / (to)) * (to))

#define K_IALLOC(pAlloc, type, ...) (type*)k_IAllocatorAlloc(pAlloc, &(type) {__VA_ARGS__}, sizeof(type))
#define K_IMALLOC_T(pAlloc, type, count) (type*)k_IAllocatorMalloc(pAlloc, sizeof(type) * count)
#define K_IZALLOC_T(pAlloc, type, count) (type*)k_IAllocatorZalloc(pAlloc, sizeof(type) * count)
#define K_IREALLOC_T(pAlloc, type, ptr, oldCount, newCount) (type*)k_IAllocatorRealloc(pAlloc, ptr, sizeof(type) * oldCount, sizeof(type) * newCount)

#define K_SIZE_MIN 2ll
#define K_SIZE_1K 1024ll
#define K_SIZE_1M (K_SIZE_1K * K_SIZE_1K)
#define K_SIZE_1G (K_SIZE_1M * K_SIZE_1K)

#define K_PTR_SIZE ((ssize_t)sizeof(void*))

ssize_t k_getPageSize(void);
static inline ssize_t k_nextPowerofTwo64(ssize_t x);
static inline bool k_isPowerOf2(ssize_t x) { return (x & (x - 1)) == 0; }
static inline void k_nullDeleter(void** pp) { *pp = NULL; }

static inline ssize_t
k_nextPowerofTwo64(ssize_t x)
{
    --x;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;

    return ++x;
}

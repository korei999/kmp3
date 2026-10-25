#pragma once

#include "IAllocator.h"

#include <stdlib.h>

typedef struct k_Gpa
{
    k_IAllocator base;
} k_Gpa;

K_NO_DISCARD static inline void* k_GpaMalloc(k_Gpa* s, ssize_t nBytes);
K_NO_DISCARD static inline void* k_GpaZalloc(k_Gpa* s, ssize_t nBytes);
K_NO_DISCARD static inline void* k_GpaRealloc(k_Gpa* s, void* p, ssize_t nBytesOld, ssize_t nBytesNew);
static inline void k_GpaFree(k_Gpa* s, void* p);
K_NO_DISCARD static inline void* k_GpaAlloc(k_Gpa* s, void* p, ssize_t size);
static inline void k_GpaInit(k_Gpa* s);
static inline k_Gpa k_GpaCreate(void);
K_NO_DISCARD static inline k_Gpa* k_GpaInst(void);

#define K_GPA_ALLOC(type, ...) (type*)k_GpaAlloc(NULL, &(type) {__VA_ARGS__}, sizeof(type))
#define K_GPA_FREE(ptr) k_GpaFree(NULL, ptr)

static inline void* k_GpaMallocStub(k_IAllocator* s, ssize_t nBytes) { return k_GpaMalloc((k_Gpa*)s, nBytes); }
static inline void* k_GpaZallocStub(k_IAllocator* s, ssize_t nBytes) { return k_GpaZalloc((k_Gpa*)s, nBytes); }
static inline void* k_GpaReallocStub(k_IAllocator* s, void* p, ssize_t nBytesOld, ssize_t nBytesNew) { return k_GpaRealloc((k_Gpa*)s, p, nBytesOld, nBytesNew); }
static inline void k_GpaFreeStub(k_IAllocator* s, void* p) { k_GpaFree((k_Gpa*)s, p); }

static const k_IAllocatorVTable k_s_vtGpa = {
    .malloc = k_GpaMallocStub,
    .zalloc = k_GpaZallocStub,
    .realloc = k_GpaReallocStub,
    .free = k_GpaFreeStub,
};

/* Devirtualization using static const (gcc/clang). */
static const k_Gpa k_s_gpa = {.base = {.pVTable = &k_s_vtGpa}};

static inline void*
k_GpaMalloc(k_Gpa* s, ssize_t nBytes)
{
    (void)s;
    return malloc(nBytes);
}

static inline void*
k_GpaZalloc(k_Gpa* s, ssize_t nBytes)
{
    (void)s;
    return calloc(1, nBytes);
}

static inline void*
k_GpaRealloc(k_Gpa* s, void* p, ssize_t nBytesOld, ssize_t nBytesNew)
{
    (void)s, (void)nBytesOld;
    return realloc(p, nBytesNew);
}

static inline void
k_GpaFree(k_Gpa* s, void* p)
{
    (void)s;
    free(p);
}

static inline void*
k_GpaAlloc(k_Gpa* s, void* p, ssize_t size)
{
    (void)s;
    void* ret = malloc(size);
    if (!ret) return NULL;
    memcpy(ret, p, size);
    return ret;
}

static inline void
k_GpaInit(k_Gpa* s)
{
    s->base.pVTable = &k_s_vtGpa;
}

static inline k_Gpa
k_GpaCreate(void)
{
    return (k_Gpa){.base = {.pVTable = &k_s_vtGpa}};
}

static inline k_Gpa*
k_GpaInst(void)
{
    return (k_Gpa*)&k_s_gpa;
}

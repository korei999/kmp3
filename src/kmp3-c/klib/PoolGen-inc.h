#include "IAllocator.h"

#ifndef K_NAME
    #error "K_NAME is not defined"
#endif

#ifndef K_TYPE
    #error "K_TYPE is not defined"
#endif

#ifndef K_INDEX_T
    #error "K_INDEX_T is not defined"
#endif

#ifndef K_DECL_MOD
    #define K_DECL_MOD static inline
#endif

#if !defined K_GEN_DECLS && !defined K_GEN_CODE
    #define K_GEN_DECLS
    #define K_GEN_CODE
#endif

#define K_METHOD(M) K_GLUE(K_NAME, M)

#ifdef K_GEN_DECLS

typedef K_INDEX_T K_POOL_HANDLE;

typedef struct K_NAME
{
    K_TYPE* pData;
    K_INDEX_T size;
    K_INDEX_T cap;
    K_INDEX_T freeListSize;
} K_NAME;

K_DECL_MOD bool K_METHOD(Init)(K_NAME* s, k_IAllocator* pAlloc, K_INDEX_T cap);
K_DECL_MOD void K_METHOD(Destroy)(K_NAME* s, k_IAllocator* pAlloc);
K_DECL_MOD K_POOL_HANDLE K_METHOD(Rent)(K_NAME* s, k_IAllocator* pAlloc, const K_TYPE* pVal);
K_DECL_MOD void K_METHOD(Return)(K_NAME* s, K_POOL_HANDLE h);
K_DECL_MOD bool K_METHOD(SetCap)(K_NAME* s, k_IAllocator* pAlloc, K_INDEX_T newCap);

K_DECL_MOD K_INDEX_T* K_METHOD(FreeList)(K_NAME* s);
K_DECL_MOD bool* K_METHOD(DeletedList)(K_NAME* s);

#endif /* K_GEN_DECLS */

#ifdef K_GEN_CODE

K_DECL_MOD bool
K_METHOD(Init)(K_NAME* s, k_IAllocator* pAlloc, K_INDEX_T cap)
{
    cap = K_MAX(8, cap);
    K_TYPE* pNew = k_IAllocatorZalloc(pAlloc, sizeof(K_TYPE)*cap + sizeof(K_INDEX_T)*cap);
    if (!pNew) return false;

    s->pData = pNew;
    s->size = 0;
    s->cap = 0;
    s->freeListSize = 0;

    return true;
}

K_DECL_MOD void
K_METHOD(Destroy)(K_NAME* s, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, s->pData);
    *s = (K_NAME){0};
}

K_DECL_MOD K_POOL_HANDLE
K_METHOD(Rent)(K_NAME* s, k_IAllocator* pAlloc, const K_TYPE* pVal)
{
    if (s->freeListSize > 0)
    {
        K_INDEX_T* pFreeList = K_METHOD(FreeList)(s);
        K_INDEX_T freeI = pFreeList[--s->freeListSize];
        s->pData[freeI] = *pVal;
        bool* pDeletedList = K_METHOD(DeletedList)(s);
        pDeletedList[freeI] = false;
        return freeI;
    }

    if (s->size >= s->cap)
    {
        if (!K_METHOD(SetCap)(s, pAlloc, K_MAX(s->size * 2, 8)))
            return -1;
    }

    s->pData[s->size++] = *pVal;
    return s->size - 1;
}

K_DECL_MOD void
K_METHOD(Return)(K_NAME* s, K_POOL_HANDLE h)
{
    assert(h >= 0 && h < s->size);
    K_INDEX_T* pFreeList = K_METHOD(FreeList)(s);
    pFreeList[s->freeListSize++] = h;
    bool* pDeletedList = K_METHOD(DeletedList)(s);
    pDeletedList[h] = true;
}

K_DECL_MOD bool
K_METHOD(SetCap)(K_NAME* s, k_IAllocator* pAlloc, K_INDEX_T newCap)
{
    K_TYPE* pNew = k_IAllocatorZalloc(pAlloc, sizeof(K_TYPE)*newCap + sizeof(K_INDEX_T)*newCap + sizeof(bool)*newCap);
    if (!pNew) return false;

    memcpy(pNew, s->pData, K_MIN(sizeof(K_TYPE)*s->size, sizeof(K_TYPE)*newCap));
    memcpy(pNew + newCap, s->pData + s->cap, K_MIN(sizeof(K_INDEX_T)*s->freeListSize, sizeof(K_INDEX_T)*newCap));
    memcpy((K_INDEX_T*)(pNew + newCap) + newCap, (K_INDEX_T*)(s->pData + s->cap) + s->cap, K_MIN(sizeof(bool)*s->size, sizeof(bool)*newCap));

    k_IAllocatorFree(pAlloc, s->pData);
    s->pData = pNew;
    s->cap = newCap;

    return true;
}

K_DECL_MOD K_INDEX_T*
K_METHOD(FreeList)(K_NAME* s)
{
    return (K_INDEX_T*)(s->pData + s->cap);
}

K_DECL_MOD bool*
K_METHOD(DeletedList)(K_NAME* s)
{
    return ((bool*)K_METHOD(FreeList)(s)) + s->cap;
}

#endif /* K_GEN_CODE */

#undef K_METHOD

#undef K_NAME
#undef K_TYPE
#undef K_DECL_MOD

#undef K_GEN_DECLS
#undef K_GEN_CODE

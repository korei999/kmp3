#include "MapDecl.h"
#include "IAllocator.h"

#include <assert.h>

#ifndef K_NAME
    #error "K_NAME is not defined"
#endif

#ifndef K_KEY_T
    #error "K_KEY_T is not defined"
#endif

#ifndef K_VALUE_T
    #error "K_VALUE_T is not defined"
#endif

#ifndef K_FN_HASH
    #error "K_FN_HASH is not defined"
#endif

#ifndef K_FN_KEY_CMP
    #error "K_FN_KEY_CMP is not defined"
#endif

#ifndef K_DECL_MOD
    #define K_DECL_MOD static inline
#endif

#if !defined K_GEN_DECLS && !defined K_GEN_CODE
    #define K_GEN_DECLS
    #define K_GEN_CODE
#endif

#define K_METHOD(M) K_GLUE(K_NAME, M)
#define K_BUCKET K_METHOD(Bucket)
#define K_MAP_RESULT K_METHOD(Result)

#ifdef K_GEN_DECLS

typedef struct K_BUCKET
{
    K_KEY_T key;
    K_VALUE_T value;
} K_BUCKET;

typedef struct K_MAP_RESULT
{
    K_BUCKET* pBucket;
    uint64_t hash;
    K_MAP_BUCKET_FLAG eFlag;
    K_MAP_RESULT_STATUS eStatus;
} K_MAP_RESULT;

typedef struct K_NAME
{
    K_BUCKET* pBuckets;
    ssize_t size; /* N occupied buckets. */
    ssize_t cap; /* Real array capacity. */
} K_NAME;

K_DECL_MOD bool K_METHOD(Init)(K_NAME* pSelf, k_IAllocator* pAlloc, ssize_t prealloc);
K_DECL_MOD K_NAME K_METHOD(Create)(k_IAllocator* pAlloc, ssize_t prealloc);
K_DECL_MOD void K_METHOD(Destroy)(K_NAME* pSelf, k_IAllocator* pAlloc);
K_DECL_MOD K_MAP_RESULT K_METHOD(Insert)(K_NAME* pSelf, k_IAllocator* pAlloc, const K_KEY_T* pKey, const K_VALUE_T* pVal);
K_DECL_MOD K_MAP_RESULT K_METHOD(TryInsert)(K_NAME* pSelf, k_IAllocator* pAlloc, const K_KEY_T* pKey, const K_VALUE_T* pVal);
K_DECL_MOD K_MAP_RESULT K_METHOD(Remove)(K_NAME* pSelf, const K_KEY_T* pKey);
K_DECL_MOD K_MAP_RESULT K_METHOD(Search)(K_NAME* pSelf, const K_KEY_T* pKey);
K_DECL_MOD ssize_t K_METHOD(FirstI)(K_NAME* pSelf);
K_DECL_MOD ssize_t K_METHOD(LastI)(K_NAME* pSelf);
K_DECL_MOD ssize_t K_METHOD(NextI)(K_NAME* pSelf, ssize_t i);
K_DECL_MOD ssize_t K_METHOD(BeginI)(K_NAME* pSelf);
K_DECL_MOD ssize_t K_METHOD(EndI)(K_NAME* pSelf);

K_DECL_MOD K_MAP_BUCKET_FLAG* K_METHOD(Flags)(K_NAME* pSelf);
K_DECL_MOD float K_METHOD(LoadFactor)(K_NAME* pSelf);
K_DECL_MOD bool K_METHOD(Rehash)(K_NAME* pSelf, k_IAllocator* pAlloc, ssize_t newCap);
K_DECL_MOD ssize_t K_METHOD(InsertionI)(K_NAME* pSelf, const K_KEY_T* pKey, uint64_t hash);
K_DECL_MOD K_MAP_RESULT K_METHOD(InsertHashed)(K_NAME* pSelf, k_IAllocator* pAlloc, const K_KEY_T* pKey, const K_VALUE_T* pVal, uint64_t hash);
K_DECL_MOD void K_METHOD(RemoveI)(K_NAME* pSelf, ssize_t i);
K_DECL_MOD K_MAP_RESULT K_METHOD(SearchHashed)(K_NAME* pSelf, const K_KEY_T* pKey, uint64_t hash);

#endif /* K_GEN_DECLS */

#ifdef K_GEN_CODE

K_DECL_MOD bool
K_METHOD(Init)(K_NAME* pSelf, k_IAllocator* pAlloc, ssize_t prealloc)
{
    assert(prealloc > 0);
    assert(pAlloc != NULL);
    assert(pSelf != NULL);

    const ssize_t cap = k_nextPowerofTwo64(K_MAX(prealloc, 8));

    pSelf->pBuckets = k_IAllocatorZalloc(pAlloc, (sizeof(K_BUCKET) + sizeof(K_MAP_BUCKET_FLAG))*cap);
    if K_UNLIKELY(!pSelf->pBuckets) return false;
    pSelf->cap = cap;
    pSelf->size = 0;

    return true;
}

K_DECL_MOD K_NAME
K_METHOD(Create)(k_IAllocator* pAlloc, ssize_t prealloc)
{
    K_NAME ret = {0};
    K_METHOD(Init)(&ret, pAlloc, prealloc);
    return ret;
}

K_DECL_MOD void
K_METHOD(Destroy)(K_NAME* pSelf, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, pSelf->pBuckets);
    pSelf->pBuckets = NULL;
    pSelf->cap = 0;
}

K_DECL_MOD K_MAP_BUCKET_FLAG*
K_METHOD(Flags)(K_NAME* s)
{
    return (K_MAP_BUCKET_FLAG*)(s->pBuckets + s->cap);
}

K_DECL_MOD float
K_METHOD(LoadFactor)(K_NAME* pSelf)
{
    assert(pSelf != NULL);
    assert(pSelf->cap > 0);
    return (float)pSelf->size / (float)(pSelf->cap);
}

K_DECL_MOD bool
K_METHOD(Rehash)(K_NAME* pSelf, k_IAllocator* pAlloc, ssize_t newCap)
{
    K_NAME mNew = K_METHOD(Create)(pAlloc, newCap);
    if K_UNLIKELY(!mNew.pBuckets) return false;

    K_MAP_BUCKET_FLAG* pEFlags = K_METHOD(Flags)(pSelf);
    for (ssize_t i = 0; i < pSelf->cap; ++i)
    {
        if (pEFlags[i] == K_MAP_BUCKET_FLAG_OCCUPIED)
            K_METHOD(Insert)(&mNew, pAlloc, &pSelf->pBuckets[i].key, &pSelf->pBuckets[i].value);
    }

    k_IAllocatorFree(pAlloc, pSelf->pBuckets);
    *pSelf = mNew;
    return true;
}

K_DECL_MOD ssize_t
K_METHOD(InsertionI)(K_NAME* pSelf, const K_KEY_T* pKey, uint64_t hash)
{
    ssize_t idx = (ssize_t)(hash & (uint64_t)(pSelf->cap - 1));
    K_MAP_BUCKET_FLAG* pEFlags = K_METHOD(Flags)(pSelf);

    while (pEFlags[idx] == K_MAP_BUCKET_FLAG_OCCUPIED)
    {
        if (K_FN_KEY_CMP(&pSelf->pBuckets[idx].key, pKey) == 0) break;
        idx = (idx + 1) & (pSelf->cap - 1);
    }

    return idx;
}

K_DECL_MOD K_MAP_RESULT
K_METHOD(InsertHashed)(K_NAME* pSelf, k_IAllocator* pAlloc, const K_KEY_T* pKey, const K_VALUE_T* pVal, uint64_t hash)
{
    if (pSelf->cap <= 0)
    {
        if K_UNLIKELY(!K_METHOD(Init)(pSelf, pAlloc, 8))
            return (K_MAP_RESULT){.eStatus = K_MAP_RESULT_STATUS_FAILED, .hash = hash};
    }
    else if (K_METHOD(LoadFactor)(pSelf) >= K_MAP_LOAD_FACTOR)
    {
        if K_UNLIKELY(!K_METHOD(Rehash)(pSelf, pAlloc, K_MAX(8, pSelf->cap * 2)))
            return (K_MAP_RESULT){.eStatus = K_MAP_RESULT_STATUS_FAILED, .hash = hash};
    }

    const ssize_t idx = K_METHOD(InsertionI)(pSelf, pKey, hash);
    K_BUCKET* pBucket = &pSelf->pBuckets[idx];
    K_MAP_BUCKET_FLAG* pEFlag = K_METHOD(Flags)(pSelf) + idx;

    *pEFlag = K_MAP_BUCKET_FLAG_OCCUPIED;
    pBucket->value = *pVal;

    if (K_FN_KEY_CMP(&pBucket->key, pKey) == 0)
        return (K_MAP_RESULT){.pBucket = pBucket, .hash = hash, .eFlag = *pEFlag, .eStatus = K_MAP_RESULT_STATUS_FOUND};

    pBucket->key = *pKey;
    ++pSelf->size;

    return (K_MAP_RESULT){.pBucket = pBucket, .hash = hash, .eFlag = *pEFlag, .eStatus = K_MAP_RESULT_STATUS_INSERTED};
}

K_DECL_MOD K_MAP_RESULT
K_METHOD(Insert)(K_NAME* pSelf, k_IAllocator* pAlloc, const K_KEY_T* pKey, const K_VALUE_T* pVal)
{
    return K_METHOD(InsertHashed)(pSelf, pAlloc, pKey, pVal, K_FN_HASH(pKey));
}

K_DECL_MOD K_MAP_RESULT
K_METHOD(TryInsert)(K_NAME* pSelf, k_IAllocator* pAlloc, const K_KEY_T* pKey, const K_VALUE_T* pVal)
{
    const uint64_t hash = K_FN_HASH(pKey);
    K_MAP_RESULT r = K_METHOD(SearchHashed)(pSelf, pKey, hash);
    if (r.eStatus == K_MAP_RESULT_STATUS_FOUND) return r;
    else return K_METHOD(InsertHashed)(pSelf, pAlloc, pKey, pVal, hash);
}

K_DECL_MOD void
K_METHOD(RemoveI)(K_NAME* pSelf, ssize_t i)
{
    assert(pSelf);
    assert(i >= 0 && i < pSelf->cap);

    K_BUCKET* pBucket = &pSelf->pBuckets[i];
    K_MAP_BUCKET_FLAG* pEFlag = K_METHOD(Flags)(pSelf) + i;

    *pBucket = (K_BUCKET){0};
    *pEFlag = K_MAP_BUCKET_FLAG_DELETED;

    --pSelf->size;
}

K_DECL_MOD K_MAP_RESULT
K_METHOD(Remove)(K_NAME* pSelf, const K_KEY_T* pKey)
{
    K_MAP_RESULT res = K_METHOD(Search(pSelf, pKey));
    if (res.eStatus == K_MAP_RESULT_STATUS_NOT_FOUND)
        return res;

    const ssize_t idx = res.pBucket - pSelf->pBuckets;
    K_METHOD(RemoveI)(pSelf, idx);
    res.eStatus = K_MAP_RESULT_STATUS_REMOVED;
    return res;
}

K_DECL_MOD K_MAP_RESULT
K_METHOD(SearchHashed)(K_NAME* pSelf, const K_KEY_T* pKey, uint64_t hash)
{
    K_MAP_RESULT res = {.eStatus = K_MAP_RESULT_STATUS_NOT_FOUND, .hash = hash};

    if (pSelf->size <= 0) return res;

    ssize_t idx = (ssize_t)(hash & (uint64_t)(pSelf->cap - 1));
    K_MAP_BUCKET_FLAG* pEFlags = K_METHOD(Flags)(pSelf);

    while (pEFlags[idx] != K_MAP_BUCKET_FLAG_NONE) /* deleted or occupied */
    {
        if (pEFlags[idx] != K_MAP_BUCKET_FLAG_DELETED &&
            K_FN_KEY_CMP(&pSelf->pBuckets[idx].key, pKey) == 0
        )
        {
            res.pBucket = (K_BUCKET*)(&pSelf->pBuckets[idx]);
            res.eFlag = pEFlags[idx];
            res.eStatus = K_MAP_RESULT_STATUS_FOUND;
            break;
        }

        idx = (idx + 1) & (pSelf->cap - 1);
    }

    return res;
}

K_DECL_MOD K_MAP_RESULT
K_METHOD(Search)(K_NAME* pSelf, const K_KEY_T* pKey)
{
    return K_METHOD(SearchHashed)(pSelf, pKey, K_FN_HASH(pKey));
}

K_DECL_MOD ssize_t
K_METHOD(FirstI)(K_NAME* pSelf)
{
    ssize_t i = 0;
    K_MAP_BUCKET_FLAG* pEFlags = K_METHOD(Flags)(pSelf);
    while (i < pSelf->cap && pEFlags[i] != K_MAP_BUCKET_FLAG_OCCUPIED)
        ++i;

    return i;
}

K_DECL_MOD ssize_t
K_METHOD(LastI)(K_NAME* pSelf)
{
    K_MAP_BUCKET_FLAG* pEFlags = K_METHOD(Flags)(pSelf);
    ssize_t i = pSelf->cap - 1;
    while (i >= 0 && pEFlags[i] != K_MAP_BUCKET_FLAG_OCCUPIED)
        --i;

    return i;
}

K_DECL_MOD ssize_t
K_METHOD(NextI)(K_NAME* pSelf, ssize_t i)
{
    K_MAP_BUCKET_FLAG* pEFlags = K_METHOD(Flags)(pSelf);
    do ++i;
    while (i < pSelf->cap && pEFlags[i] != K_MAP_BUCKET_FLAG_OCCUPIED);

    return i;
}

K_DECL_MOD ssize_t
K_METHOD(BeginI)(K_NAME* pSelf)
{
    return K_METHOD(FirstI)(pSelf);
}

K_DECL_MOD ssize_t
K_METHOD(EndI)(K_NAME* pSelf)
{
    return pSelf->cap;
}

#endif /* K_GEN_CODE */

#undef K_BUCKET
#undef K_METHOD
#undef K_MAP_RESULT

#undef K_NAME
#undef K_KEY_T
#undef K_VALUE_T
#undef K_FN_HASH
#undef K_FN_KEY_CMP
#undef K_DECL_MOD

#undef K_DECL_MOD

#undef K_GEN_DECLS
#undef K_GEN_CODE

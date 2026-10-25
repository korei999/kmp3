#include "IAllocator.h"

#ifndef K_NAME
    #error "K_NAME is not defined"
#endif

#ifndef K_TYPE
    #error "K_TYPE is not defined"
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

typedef struct K_NAME
{
    K_TYPE* pData;
    ssize_t size;
    ssize_t cap;
} K_NAME;

K_DECL_MOD bool K_METHOD(Init)(K_NAME* s, k_IAllocator* pAlloc, ssize_t cap);
K_DECL_MOD void K_METHOD(Destroy)(K_NAME* s, k_IAllocator* pAlloc);
K_DECL_MOD bool K_METHOD(Grow)(K_NAME* s, k_IAllocator* pAlloc, ssize_t newCap);
K_DECL_MOD ssize_t K_METHOD(Push)(K_NAME* s, k_IAllocator* pAlloc, const K_TYPE* pVal);
K_DECL_MOD ssize_t K_METHOD(PushVal)(K_NAME* s, k_IAllocator* pAlloc, const K_TYPE val);
K_DECL_MOD ssize_t K_METHOD(PushMany)(K_NAME* s, k_IAllocator* pAlloc, const K_TYPE* p, ssize_t size);
K_DECL_MOD K_TYPE* K_METHOD(Pop)(K_NAME* s);
K_DECL_MOD void K_METHOD(PopAsLast)(K_NAME* s, ssize_t i, K_TYPE* pPopDestOrNull);
K_DECL_MOD void K_METHOD(Remove)(K_NAME* s, ssize_t i);
K_DECL_MOD bool K_METHOD(Shrink)(K_NAME* s, k_IAllocator* pAlloc, ssize_t newCap);
K_DECL_MOD bool K_METHOD(PopShrink)(K_NAME* s, k_IAllocator* pAlloc);
K_DECL_MOD bool K_METHOD(SetSize)(K_NAME* s, k_IAllocator* pAlloc, ssize_t newSize);
K_DECL_MOD bool K_METHOD(SetCap)(K_NAME* s, k_IAllocator* pAlloc, ssize_t newCap);
K_DECL_MOD K_TYPE K_METHOD(Get)(K_NAME* s, ssize_t i);
K_DECL_MOD K_TYPE* K_METHOD(GetP)(K_NAME* s, ssize_t i);
K_DECL_MOD const K_TYPE* K_METHOD(GetPConst)(const K_NAME* s, ssize_t i);
K_DECL_MOD void K_METHOD(Set)(K_NAME* s, ssize_t i, const K_TYPE* p);
K_DECL_MOD void K_METHOD(SetVal)(K_NAME* s, ssize_t i, const K_TYPE val);
K_DECL_MOD K_TYPE* K_METHOD(FirstP)(K_NAME* s);
K_DECL_MOD K_TYPE* K_METHOD(LastP)(K_NAME* s);

#endif /* K_GEN_DECLS */

#ifdef K_GEN_CODE

K_DECL_MOD bool
K_METHOD(Init)(K_NAME* s, k_IAllocator* pAlloc, ssize_t cap)
{
    K_TYPE* pNew = NULL;
    if (cap > 0)
    {
        pNew = K_IMALLOC_T(pAlloc, K_TYPE, cap);
        if (!pNew) return false;
    }

    s->pData = pNew;
    s->size = 0;
    s->cap = cap;

    return true;
}

K_DECL_MOD void
K_METHOD(Destroy)(K_NAME* s, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, s->pData);
    *s = (K_NAME){0};
}

K_DECL_MOD ssize_t
K_METHOD(PushVal)(K_NAME* s, k_IAllocator* pAlloc, const K_TYPE val)
{
    return K_METHOD(Push)(s, pAlloc, &val);
}

K_DECL_MOD ssize_t
K_METHOD(Push)(K_NAME* s, k_IAllocator* pAlloc, const K_TYPE* pVal)
{
    if (s->size >= s->cap)
    {
        if (!K_METHOD(Grow)(s, pAlloc, K_MAX(2, s->cap * 2)))
            return K_NPOS;
    }

    s->pData[s->size++] = *(K_TYPE*)pVal;
    return s->size - 1;
}

K_DECL_MOD ssize_t
K_METHOD(PushMany)(K_NAME* s, k_IAllocator* pAlloc, const K_TYPE* p, ssize_t size)
{
    if (size <= 0) return s->size;
    if (s->size + size > s->cap)
    {
        ssize_t newCap = s->cap * 2;
        if (s->size + size > newCap) newCap = s->size + size;
        if (!K_METHOD(Grow)(s, pAlloc, newCap))
            return K_NPOS;
    }

    memcpy(s->pData + s->size, p, sizeof(*p) * size);
    s->size += size;
    return s->size - size;
}

K_DECL_MOD K_TYPE*
K_METHOD(Pop)(K_NAME* s)
{
    assert(s->size > 0);
    return s->pData + --s->size;
}

K_DECL_MOD void
K_METHOD(PopAsLast)(K_NAME* s, ssize_t i, K_TYPE* pPopDestOrNull)
{
    assert(s->size > 0);
    if (pPopDestOrNull) memcpy(pPopDestOrNull, s->pData + i, sizeof(*s->pData));
    memcpy(s->pData + i, s->pData + --s->size, sizeof(*s->pData));
}

K_DECL_MOD void
K_METHOD(Remove)(K_NAME* s, ssize_t i)
{
    memcpy(s->pData + i, s->pData + --s->size, sizeof(*s->pData));
}

K_DECL_MOD bool
K_METHOD(PopShrink)(K_NAME* s, k_IAllocator* pAlloc)
{
    assert(s->size > 0);
    if (s->size <= s->cap >> 2)
    {
        if (!K_METHOD(Shrink)(s, pAlloc, s->cap >> 1))
            return false;
    }
    --s->size;
    return true;
}

K_DECL_MOD bool
K_METHOD(Grow)(K_NAME* s, k_IAllocator* pAlloc, ssize_t newCap)
{
    K_TYPE* pNew = K_IREALLOC_T(pAlloc, K_TYPE, s->pData, s->size, newCap);
    if (!pNew) return false;

    s->pData = pNew;
    s->cap = newCap;

    return true;
}

K_DECL_MOD bool
K_METHOD(Shrink)(K_NAME* s, k_IAllocator* pAlloc, ssize_t newCap)
{
    K_TYPE* pNew = K_IREALLOC_T(pAlloc, K_TYPE, s->pData, newCap, newCap);
    if (!pNew) return false;

    s->pData = pNew;
    s->cap = newCap;
    if (s->size > s->cap) s->size = s->cap;

    return true;
}

K_DECL_MOD bool
K_METHOD(SetSize)(K_NAME* s, k_IAllocator* pAlloc, ssize_t newSize)
{
    assert(newSize >= 0);

    if (newSize > s->cap)
    {
        K_TYPE* pNew = K_IREALLOC_T(pAlloc, K_TYPE, s->pData, s->cap, newSize);
        if (!pNew) return false;

        s->pData = pNew;
    }

    s->size = newSize;
    return true;
}

K_DECL_MOD bool
K_METHOD(SetCap)(K_NAME* s, k_IAllocator* pAlloc, ssize_t newCap)
{
    K_TYPE* pNew = K_IREALLOC_T(pAlloc, K_TYPE, s->pData, K_MIN(newCap, s->size), newCap);
    if (!pNew) return false;

    s->pData = pNew;
    s->cap = newCap;
    if (s->size > s->cap) s->size = s->cap;

    return true;
}

K_DECL_MOD K_TYPE
K_METHOD(Get)(K_NAME* s, ssize_t i)
{
    assert(i >= 0 && i < s->size);
    return s->pData[i];
}

K_DECL_MOD K_TYPE*
K_METHOD(GetP)(K_NAME* s, ssize_t i)
{
    assert(i >= 0 && i < s->size);
    return s->pData + i;
}

K_DECL_MOD const K_TYPE*
K_METHOD(GetPConst)(const K_NAME* s, ssize_t i)
{
    assert(i >= 0 && i < s->size);
    return (const K_TYPE*)(s->pData + i);
}

K_DECL_MOD void
K_METHOD(Set)(K_NAME* s, ssize_t i, const K_TYPE* p)
{
    assert(i >= 0 && i < s->size);
    memcpy(s->pData + i, p, sizeof(*p));
}

K_DECL_MOD void
K_METHOD(SetVal)(K_NAME* s, ssize_t i, const K_TYPE val)
{
    assert(i >= 0 && i < s->size);
    memcpy(s->pData + i, &val, sizeof(val));
}

K_DECL_MOD K_TYPE*
K_METHOD(FirstP)(K_NAME* s)
{
    return K_METHOD(GetP)(s, 0);
}

K_DECL_MOD K_TYPE*
K_METHOD(LastP)(K_NAME* s)
{
    return K_METHOD(GetP)(s, s->size - 1);
}

#endif /* K_GEN_CODE */

#undef K_METHOD

#undef K_NAME
#undef K_TYPE
#undef K_DECL_MOD

#undef K_GEN_DECLS
#undef K_GEN_CODE

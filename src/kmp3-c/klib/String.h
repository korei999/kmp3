#pragma once

#include "IAllocator.h"
#include "StringView.h"

#define K_STRING_SMALL_SIZE (K_PTR_SIZE*2)

/* String with small buffer optimization. */
typedef struct k_String
{
    struct
    {
        union
        {
            char aBuff[K_STRING_SMALL_SIZE];
            struct {
                char* pData;
                ssize_t size;
            } ptr;
        };
        ssize_t cap;
    } priv;
} k_String;

K_NO_DISCARD static inline k_String k_StringCreateSmall(void);
K_NO_DISCARD k_String k_StringCreateSmallSv(const k_StringView sv);
K_NO_DISCARD k_String k_StringCreate(k_IAllocator* pAlloc, const char* pData, ssize_t size);
K_NO_DISCARD static inline k_String k_StringCreateNts(k_IAllocator* pAlloc, const char* nts);
K_NO_DISCARD static inline k_String k_StringCreateSv(k_IAllocator* pAlloc, const k_StringView sv);
void k_StringDestroy(k_String* s, k_IAllocator* pAlloc);
static inline ssize_t k_StringCap(const k_String* s);
static inline ssize_t k_StringSize(const k_String* s);
static inline char* k_StringData(k_String* s);
static inline const char* k_StringDataConst(const k_String* s);
static inline k_StringView k_StringToSv(const k_String* s);
bool k_StringReallocWith(k_String* s, k_IAllocator* pAlloc, const k_StringView svWith);
bool k_StringPush(k_String* s, k_IAllocator* pAlloc, const char* pData, ssize_t size);
bool k_StringPushFront(k_String* s, k_IAllocator* pAlloc, const char* pData, ssize_t size);
static inline bool k_StringPushSv(k_String* s, k_IAllocator* pAlloc, const k_StringView sv);
static inline bool k_StringPushFrontSv(k_String* s, k_IAllocator* pAlloc, const k_StringView sv);
static inline char k_StringGet(const k_String* s, ssize_t i);
static inline void k_StringSet(k_String* s, ssize_t i, char c);

static inline bool k_StringEq(const k_String* pL, const k_String* pR);
static inline ssize_t k_StringCmp(const k_String* l, const k_String* r);
static inline ssize_t k_StringCmpRev(const k_String* l, const k_String* r);
static inline uint64_t k_StringHash(const k_String* pSv);

static inline k_String
k_StringCreateSmall(void)
{
    return (k_String){.priv.aBuff = {0}, .priv.cap = K_STRING_SMALL_SIZE};
}

static inline k_String
k_StringCreateNts(k_IAllocator* pAlloc, const char* nts)
{
    return k_StringCreate(pAlloc, nts, strlen(nts));
}

static inline k_String
k_StringCreateSv(k_IAllocator* pAlloc, const k_StringView sv)
{
    return k_StringCreate(pAlloc, sv.pData, sv.size);
}

static inline ssize_t
k_StringCap(const k_String* s)
{
    return s->priv.cap;
}

static inline ssize_t
k_StringSize(const k_String* s)
{
    if (s->priv.cap > K_STRING_SMALL_SIZE) return s->priv.ptr.size;
    else return strnlen(s->priv.aBuff, K_STRING_SMALL_SIZE);
}

static inline char*
k_StringData(k_String* s)
{
    if (s->priv.cap > K_STRING_SMALL_SIZE) return s->priv.ptr.pData;
    else return s->priv.aBuff;
}

static inline const char*
k_StringDataConst(const k_String* s)
{
    if (s->priv.cap > K_STRING_SMALL_SIZE) return s->priv.ptr.pData;
    else return s->priv.aBuff;
}

static inline k_StringView
k_StringToSv(const k_String* s)
{
    k_StringView sv;

    if (s->priv.cap > K_STRING_SMALL_SIZE)
    {
        sv.pData = s->priv.ptr.pData;
        sv.size = s->priv.ptr.size;
    }
    else
    {
        sv.pData = (char*)s->priv.aBuff;
        sv.size = strnlen(s->priv.aBuff, K_STRING_SMALL_SIZE);
    }

    return sv;
}

static inline bool
k_StringPushSv(k_String* s, k_IAllocator* pAlloc, const k_StringView sv)
{
    return k_StringPush(s, pAlloc, sv.pData, sv.size);
}

static inline bool
k_StringPushFrontSv(k_String* s, k_IAllocator* pAlloc, const k_StringView sv)
{
    return k_StringPushFront(s, pAlloc, sv.pData, sv.size);
}

static inline char
k_StringGet(const k_String* s, ssize_t i)
{
    assert(i >= 0 && i < k_StringSize(s));
    return k_StringDataConst(s)[i];
}

static inline void
k_StringSet(k_String* s, ssize_t i, char c)
{
    assert(i >= 0 && i < k_StringSize(s));
    k_StringData(s)[i] = c;
}

static inline bool
k_StringEq(const k_String* pL, const k_String* pR)
{
    return k_StringViewEq(k_StringToSv(pL), k_StringToSv(pR));
}

static inline ssize_t
k_StringCmp(const k_String* l, const k_String* r)
{
    const k_StringView sL = k_StringToSv(l), sR = k_StringToSv(r);
    return k_StringViewCmp(&sL, &sR);
}

static inline ssize_t
k_StringCmpRev(const k_String* l, const k_String* r)
{
    const k_StringView sL = k_StringToSv(l), sR = k_StringToSv(r);
    return k_StringViewCmpRev(&sL, &sR);
}

static inline uint64_t
k_StringHash(const k_String* pSv)
{
    const k_StringView sL = k_StringToSv(pSv);
    return k_StringViewHash(&sL);
}

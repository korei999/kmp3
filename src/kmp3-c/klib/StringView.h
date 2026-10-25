#pragma once

#include "hash.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
    char* pData;
    ssize_t size;
} k_StringView;

#define K_NTS(nts) (k_StringView){(char*)nts, strlen(nts)}
#define K_SV(nts) (k_StringView){(char*)nts, (ssize_t)(sizeof(nts) - 1)}

static inline bool k_oneOfChars(char c, const k_StringView svChars);
static inline bool k_noneOfChars(char c, const k_StringView svChars);

ssize_t k_StringViewCmp(const k_StringView* l, const k_StringView* r);
ssize_t k_StringViewCmpRev(const k_StringView* l, const k_StringView* r);
static inline uint64_t k_StringViewHash(const k_StringView* pSv);

static inline bool k_StringViewEq(const k_StringView s, const k_StringView r);
ssize_t k_StringViewCharAt(const k_StringView s, char c);
ssize_t k_StringViewCharAtRev(const k_StringView s, char c); /* Last to first order. */
ssize_t k_StringViewSubStringAt(const k_StringView s, const k_StringView svSub);
bool k_StringViewHasSv(const k_StringView s, const k_StringView svSub);
bool k_StringViewHasChar(const k_StringView s, char c);
bool k_StringViewHasOneOf(const k_StringView s, const k_StringView svOneOf);
bool k_StringViewStartsWith(const k_StringView s, const k_StringView svWith);
bool k_StringViewEndsWith(const k_StringView s, const k_StringView svWith);

k_StringView k_StringViewPathEnding(const k_StringView s);

static inline k_StringView k_StringViewSubString(const k_StringView s, ssize_t startI, ssize_t size);
static inline k_StringView k_StringViewSubString1(const k_StringView s, ssize_t startI);

static inline int k_StringViewToInt(const k_StringView s, int base);
static inline int64_t k_StringViewToI64(const k_StringView s, int base);
static inline uint64_t k_StringViewToU64(const k_StringView s, int base);
static inline float k_StringViewToFloat(const k_StringView s);
static inline double k_StringViewToDouble(const k_StringView s);

static inline char k_StringViewGet(const k_StringView* s, ssize_t i);
static inline void k_StringViewSet(const k_StringView* s, ssize_t i, char c);

/* Impl */

static inline bool
k_oneOfChars(char c, const k_StringView svChars)
{
    for (ssize_t i = 0; i < svChars.size; ++i)
        if (c == svChars.pData[i])
            return true;

    return false;
}

static inline bool
k_noneOfChars(char c, const k_StringView svChars)
{
    for (ssize_t i = 0; i < svChars.size; ++i)
        if (c == svChars.pData[i])
            return false;

    return true;
}

static inline uint64_t
k_StringViewHash(const k_StringView* pSv)
{
#ifdef K_SSE4_2
    return k_hash_crc32((uint8_t*)pSv->pData, pSv->size, 0);
#else
    return k_hash_mulXor((const uint8_t*)pSv->pData, pSv->size, 0);
#endif
}

static inline bool
k_StringViewEq(const k_StringView s, const k_StringView r)
{
    if (s.size != r.size) return false;
    else return strncmp(s.pData, r.pData, K_MIN(s.size, r.size)) == 0;
}

static inline k_StringView
k_StringViewSubString(const k_StringView s, ssize_t startI, ssize_t size)
{
    assert(startI + size <= s.size);
    assert((startI >= 0 && startI < s.size) || size <= 0);
    return (k_StringView){s.pData + startI, K_MAX(0, size)};
}

static inline k_StringView
k_StringViewSubString1(const k_StringView s, ssize_t startI)
{
    assert(startI <= s.size && "(out of range)");
    return k_StringViewSubString(s, startI, K_MAX(0, s.size - startI));
}

static inline int
k_StringViewToInt(const k_StringView s, int base)
{
    char* pEnd = s.pData + s.size;
    return (int)strtol(s.pData, &pEnd, base);
}

static inline int64_t
k_StringViewToI64(const k_StringView s, int base)
{
    char* pEnd = s.pData + s.size;
    return strtoll(s.pData, &pEnd, base);
}

static inline uint64_t
k_StringViewToU64(const k_StringView s, int base)
{
    char* pEnd = s.pData + s.size;
    return strtoull(s.pData, &pEnd, base);
}

static inline float
k_StringViewToFloat(const k_StringView s)
{
    char* pEnd = s.pData + s.size;
    return strtof(s.pData, &pEnd);
}

static inline double
k_StringViewToDouble(const k_StringView s)
{
    char* pEnd = s.pData + s.size;
    return strtod(s.pData, &pEnd);
}

static inline char
k_StringViewGet(const k_StringView* s, ssize_t i)
{
    assert(i >= 0 && i < s->size);
    return s->pData[i];
}

static inline void
k_StringViewSet(const k_StringView* s, ssize_t i, char c)
{
    assert(i >= 0 && i < s->size);
    s->pData[i] = c;
}

/* Impl end */

#include "StringView.h"

ssize_t
k_StringViewCmp(const k_StringView* l, const k_StringView* r)
{
    if (l->pData == r->pData) return 0;

    const ssize_t minLen = l->size <= r->size ? l->size : r->size;
    if (minLen <= 0) goto cmpSizes;

    const int res = strncmp(l->pData, r->pData, minLen);
    if (res == 0)
    {
cmpSizes:
        if (l->size == r->size) return 0;
        else if (l->size < r->size) return -1;
        else return 1;
    }
    else
    {
        return res;
    }
}

ssize_t
k_StringViewCmpRev(const k_StringView* l, const k_StringView* r)
{
    if (l->pData == r->pData) return 0;

    const ssize_t minLen = l->size <= r->size ? l->size : r->size;
    if (minLen <= 0) goto cmpSizes;

    const int res = strncmp(r->pData, l->pData, minLen);
    if (res == 0)
    {
cmpSizes:
        if (r->size == l->size) return 0;
        else if (r->size < l->size) return -1;
        else return 1;
    }
    else
    {
        return res;
    }
}

ssize_t
k_StringViewCharAt(const k_StringView s, char c)
{
    char* pMem = memchr(s.pData, c, s.size);
    if (!pMem) return -1;
    else return pMem - s.pData;
}

ssize_t
k_StringViewCharAtRev(const k_StringView s, char c)
{
    for (ssize_t i = s.size - 1; i >= 0; --i)
        if (s.pData[i] == c) return i;

    return -1;
}

ssize_t
k_StringViewSubStringAt(const k_StringView s, const k_StringView r)
{
    if (s.size < r.size || s.size == 0 || r.size == 0)
        return -1;

#if defined __unix__

    const void* ptr = memmem(s.pData, s.size, r.pData, r.size);
    if (ptr) return (const char*)ptr - s.pData;

#else

    for (ssize_t i = 0; i < s.size - r.size + 1; ++i)
    {
        const k_StringView svSub = {(char*)(s.pData + i), r.size};
        if (!k_StringViewCmp(&svSub, &r))
            return i;
    }

#endif

    return -1;
}

bool
k_StringViewHasSv(const k_StringView s, const k_StringView r)
{
    if (s.size < r.size || s.size == 0 || r.size == 0)
        return false;

#if defined __unix__

    return memmem(s.pData, s.size, r.pData, r.size) != NULL;

#else

    for (ssize_t i = 0; i < s.size - r.size + 1; ++i)
    {
        const k_StringView svSub = {(char*)(s.pData + i), r.size};
        if (!k_StringViewCmp(&svSub, &r))
            return true;
    }

    return false;

#endif
}

bool
k_StringViewHasChar(const k_StringView s, char c)
{
    if (s.size < 0 || !s.pData) return false;
    return memchr(s.pData, c, s.size) != NULL;
}

bool
k_StringViewHasOneOf(const k_StringView s, const k_StringView svOneOf)
{
    for (ssize_t i = 0; i < s.size; ++i)
    {
        for (ssize_t j = 0; j < svOneOf.size; ++j)
            if (s.pData[i] == svOneOf.pData[j])
                return true;
    }

    return false;
}

bool
k_StringViewStartsWith(const k_StringView sv, const k_StringView svWith)
{
    if (sv.size < svWith.size) return false;
    k_StringView svSub = k_StringViewSubString(sv, 0, svWith.size);
    return !k_StringViewCmp(&svWith, &svSub);
}

bool
k_StringViewEndsWith(const k_StringView s, const k_StringView svWith)
{
    if (s.size < svWith.size) return false;
    k_StringView svSub = k_StringViewSubString1(s, s.size - svWith.size);
    return !k_StringViewCmp(&svWith, &svSub);
}

k_StringView
k_StringViewPathEnding(const k_StringView s)
{
    ssize_t off = k_StringViewCharAtRev(s, '/');
    if (off == -1 || (off + 1) == s.size) return s;

    return (k_StringView){.pData = (char*)s.pData + off + 1, .size = s.size - off - 1};
}

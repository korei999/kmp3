#pragma once

#include "klib/common.h"

#define K_UTF_INVALID 0xFFFD

static inline int
k_utf8_decode(const char* pStr, ssize_t strSize, int32_t* pCodePoint, int* pErr)
{
    static const unsigned char lens[] = {
        /* 0XXXX */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        /* 10XXX */ 0, 0, 0, 0, 0, 0, 0, 0, /* invalid */
        /* 110XX */ 2, 2, 2, 2,
        /* 1110X */ 3, 3,
        /* 11110 */ 4,
        /* 11111 */ 0, /* invalid */
    };
    static const unsigned char leading_mask[] = {0x7F, 0x1F, 0x0F, 0x07};
    static const unsigned int overlong[] = {0x0, 0x80, 0x0800, 0x10000};

    const unsigned char* s = (const unsigned char*)pStr;
    int len = lens[*s >> 3];
    *pCodePoint = K_UTF_INVALID;
    *pErr = 1;
    if (len == 0)
        return 1;

    int32_t cp = s[0] & leading_mask[len - 1];
    for (int i = 1; i < len; ++i)
    {
        if (i >= strSize || s[i] == '\0' || (s[i] & 0xC0) != 0x80)
            return i;
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    /* out of range, surrogate, overlong encoding */
    if (cp > 0x10FFFF || (cp >> 11) == 0x1B || cp < overlong[len - 1])
        return len;

    *pErr = 0;
    *pCodePoint = cp;
    return len;
}

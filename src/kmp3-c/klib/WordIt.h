#pragma once

#include "StringView.h"

typedef struct k_WordIt
{
    k_StringView sv;
    k_StringView svSeparators;
    ssize_t startI, endI;
} k_WordIt;

static inline k_WordIt k_WordItCreate(const k_StringView sv, const k_StringView svSeparators);
static inline bool k_WordItDone(const k_WordIt* s);
static inline void k_WordItNext(k_WordIt* s);
static inline k_StringView k_WordItToSv(const k_WordIt* s);

static inline k_WordIt
k_WordItCreate(const k_StringView sv, const k_StringView svSeparators)
{
    k_WordIt it = {.sv = sv, .svSeparators = svSeparators, .startI = 0, .endI = 0};
    k_WordItNext(&it);
    return it;
}

static inline bool
k_WordItDone(const k_WordIt* s)
{
    if (s->startI >= s->sv.size) return true;
    else return false;
}

static inline void
k_WordItNext(k_WordIt* s)
{
    /* Skip separators. */
    while (s->endI < s->sv.size)
    {
        if (!k_oneOfChars(s->sv.pData[s->endI], s->svSeparators))
            break;
        ++s->endI;
    }

    s->startI = s->endI;

    while (s->endI < s->sv.size)
    {
        if (k_oneOfChars(s->sv.pData[s->endI], s->svSeparators))
            return;
        ++s->endI;
    }
}

static inline k_StringView
k_WordItToSv(const k_WordIt* s)
{
    return k_StringViewSubString(s->sv, s->startI, s->endI - s->startI);
}

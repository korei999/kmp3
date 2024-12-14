#pragma once

#include "adt/Arena.hh"
#include "adt/String.hh"
#include "adt/print.hh"

using namespace adt;

struct TextBuff
{
    Arena* pAlloc {};
    char* pData {};
    u32 size {};
    u32 capacity {};

    TextBuff() = default;
    TextBuff(Arena* _pAlloc) : pAlloc(_pAlloc) {}
};

inline void
TextBuffPush(TextBuff* s, const String str)
{
    if (s->size + str.size > s->capacity)
    {
        u32 newCap = str.size + s->capacity*2;
        s->pData = (char*)realloc(s->pAlloc, s->pData, 1, newCap);
        s->capacity = newCap;
    }

    memcpy(s->pData + s->size, str.pData, str.size);
    s->size += str.size;
}

inline void
TextBuffReset(TextBuff* s)
{
    /*memset(s->pData, 0, s->size);*/
    s->size = 0;
}

inline void
TextBuffFlush(TextBuff* s)
{
    write(STDOUT_FILENO, s->pData, s->size);
    fflush(stdout);
    TextBuffReset(s);
}

inline void
TextCursorAt(TextBuff* s, int x, int y)
{
    char aBuff[64] {};
    print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[H\x1b[{}C\x1b[{}B", x, y);
    TextBuffPush(s, aBuff);
}

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
TextBuffPush(TextBuff* s, const char* pBuff, u32 buffSize)
{
    if (buffSize + s->size >= s->capacity)
    {
        const u32 newCap = utils::maxVal(buffSize + s->size, s->capacity*2);
        s->pData = (char*)realloc(s->pAlloc, s->pData, newCap, 1);
        s->capacity = newCap;
    }

    memcpy(s->pData + s->size, pBuff, buffSize);
    s->size += buffSize;
}

inline void
TextBuffPush(TextBuff* s, const String sBuff)
{
    TextBuffPush(s, sBuff.pData, sBuff.size);
}

inline void
TextBuffReset(TextBuff* s)
{
    s->pData = {};
    s->size = {};
    s->capacity = {};
}

inline void
TextBuffFlush(TextBuff* s)
{
    write(STDOUT_FILENO, s->pData, s->size);
    fflush(stdout);
}

inline void
TextBuffCursorAt(TextBuff* s, int x, int y)
{
    char aBuff[64] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[H\x1b[{}C\x1b[{}B", x, y);
    TextBuffPush(s, aBuff, n);
}

inline void
TextBuffClear(TextBuff* s)
{
    char aBuff[16] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[2J");
    TextBuffPush(s, aBuff, n);
}

inline void
TextBuffHideCursor(TextBuff* s, bool bHide)
{
    if (bHide) TextBuffPush(s, "\x1b[?25l");
    else TextBuffPush(s, "\x1b[?25h");
}

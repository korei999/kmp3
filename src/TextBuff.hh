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
TextBuffPush(TextBuff* s, const char* pBuff, const u32 buffSize)
{
    if (buffSize + s->size >= s->capacity)
    {
        const u32 newCap = utils::maxVal(buffSize + s->size, s->capacity*2);
        s->pData = (char*)realloc(s->pAlloc, s->pData, newCap, 1);
        s->capacity = newCap;
    }

    strncpy(s->pData + s->size, pBuff, buffSize);
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
TextBuffMoveTopLeft(TextBuff* s)
{
    TextBuffPush(s, "\x1b[H");
}

inline void
TextBuffUp(TextBuff* s, int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}A", steps);
    TextBuffPush(s, aBuff, n);
}

inline void
TextBuffDown(TextBuff* s, int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}B", steps);
    TextBuffPush(s, aBuff, n);
}

inline void
TextBuffForward(TextBuff* s, int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}C", steps);
    TextBuffPush(s, aBuff, n);
}

inline void
TextBuffBack(TextBuff* s, int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}D", steps);
    TextBuffPush(s, aBuff, n);
}

inline void
TextBuffMove(TextBuff* s, int x, int y)
{
    char aBuff[64] {};
    /*u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[H\x1b[{}C\x1b[{}B", x, y);*/
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{};{}H", y + 1, x + 1);
    TextBuffPush(s, aBuff, n);
}

inline void
TextBuffClearDown(TextBuff* s)
{
    TextBuffPush(s, "\x1b[0J");
}

inline void
TextBuffClearUp(TextBuff* s)
{
    TextBuffPush(s, "\x1b[1J");
}

inline void
TextBuffClear(TextBuff* s)
{
    TextBuffPush(s, "\x1b[2J");
}

inline void
TextBuffHideCursor(TextBuff* s, bool bHide)
{
    if (bHide) TextBuffPush(s, "\x1b[?25l");
    else TextBuffPush(s, "\x1b[?25h");
}

inline void
TextBuffMovePush(TextBuff* s, int x, int y, const String str)
{
    TextBuffMove(s, x, y);
    TextBuffPush(s, str);
}

inline void
TextBuffMovePush(TextBuff* s, int x, int y, const char* pBuff, const u32 size)
{
    TextBuffMove(s, x, y);
    TextBuffPush(s, pBuff, size);
}

inline void
TextBuffPushGlyphs(TextBuff* s, const String str, const u32 nColumns)
{
    char* pBuff = (char*)zalloc(s->pAlloc, str.size*2, 1);
    u32 off = 0;

    u32 totalWidth = 0;
    for (u32 i = 0; i < str.size && totalWidth < nColumns; )
    {
        wchar_t wc {};
        int wclen = mbtowc(&wc, &str[i], str.size - i);
        totalWidth += wcwidth(wc);
        i += wclen;

        off += wctomb(pBuff + off, wc);
    }

    TextBuffPush(s, pBuff);
}

inline void
TextBuffMovePushGlyphs(TextBuff* s, int x, int y, const String str, const u32 nColumns)
{
    TextBuffMove(s, x, y);
    TextBuffPushGlyphs(s, str, nColumns);
}

inline void
TextBuffPushWideString(TextBuff* s, const wchar_t* pwBuff, const u32 wBuffSize)
{
    char* pBuff = (char*)zalloc(s->pAlloc, wBuffSize, sizeof(wchar_t));
    wcstombs(pBuff, pwBuff, wBuffSize);
    TextBuffPush(s, pBuff);
}

inline void
TextBuffMovePushWideString(TextBuff* s, int x, int y, const wchar_t* pwBuff, const u32 wBuffSize)
{
    TextBuffMove(s, x, y);
    TextBuffPushWideString(s, pwBuff, wBuffSize);
}

inline void
TextBuffClearKittyImages(TextBuff* s)
{
    TextBuffPush(s, "\x1b_Ga=d,d=A\x1b\\");
}

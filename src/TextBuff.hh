#pragma once

#include "adt/Arena.hh"
#include "adt/String.hh"
#include "adt/logs.hh"
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
    LOG("copy: from: {}, to: {}, end: {}\n\t\tsize: {}, currSize: {}, cap: {}\n",
        (void*)pBuff, (void*)&s->pData[s->size], (void*)(s->pData + s->size + buffSize),
        buffSize, s->size, s->capacity
    );

    if (s->size == 0) LOG_WARN("FIRST PUSH\n");

    if ((s->size + buffSize) >= s->capacity)
    {
        const u32 newCap = utils::maxVal(buffSize, s->capacity*2);

        LOG_GOOD("asking realloc for {} bytes\n", newCap);
        s->pData = (char*)realloc(s->pAlloc, s->pData, newCap, 1);

        /*s->pData = (char*)alloc(s->pAlloc, newCap, sizeof(*s->pData));*/

        s->capacity = newCap;
    }

    if (pBuff + buffSize > s->pData + s->size &&
        pBuff + buffSize <= s->pData + s->capacity)
    {
        LOG_BAD("OVERLAP\n");
    }

    /*strncpy(s->pData + s->size, pBuff, size);*/
    /*memmove(s->pData + s->size, pBuff, buffSize);*/
    memcpy(s->pData + s->size, pBuff, buffSize);
    s->size += buffSize;
}

inline void
TextBuffPushString(TextBuff* s, const String sBuff)
{
    TextBuffPush(s, sBuff.pData, sBuff.size);
}

inline void
TextBuffReset(TextBuff* s)
{
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

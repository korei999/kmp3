#pragma once

#include "adt/Arena.hh"
#include "adt/String.hh"
#include "adt/print.hh"

using namespace adt;

/*
Erases part of the line.
If n is 0 (or missing), clear from cursor to the end of the line.
If n is 1, clear from cursor to beginning of the line.
If n is 2, clear entire line. Cursor position does not change. 
*/

enum class TEXT_BUFF_ARG : u8
{
    TO_END, TO_BEGINNING, EVERYTHING
};

struct TextBuff
{
    Arena* pAlloc {};
    char* pData {};
    u32 size {};
    u32 capacity {};

    TextBuff() = default;
    TextBuff(Arena* _pAlloc) : pAlloc(_pAlloc) {}

    void push(const char* pBuff, const u32 buffSize);
    void push(const String sBuff);
    void reset();
    void flush();
    void moveTopLeft();
    void up(int steps);
    void down(int steps);
    void forward(int steps);
    void back(int steps);
    void move(int x, int y);
    void clearDown();
    void clearUp();
    void clear();
    void clearLine(TEXT_BUFF_ARG eArg);
    void moveClearLine(int x, int y, TEXT_BUFF_ARG eArg);
    void hideCursor(bool bHide);
    void movePush(int x, int y, const String str);
    void movePush(int x, int y, const char* pBuff, const u32 size);
    void pushGlyphs(const String str, const u32 nColumns);
    void movePushGlyphs(int x, int y, const String str, const u32 nColumns);
    void pushWideString(const wchar_t* pwBuff, const u32 wBuffSize);
    void movePushWideString(int x, int y, const wchar_t* pwBuff, const u32 wBuffSize);
    void clearKittyImages();
};

inline void
TextBuff::push(const char* pBuff, const u32 buffSize)
{
    if (buffSize + this->size >= this->capacity)
    {
        const u32 newCap = utils::maxVal(buffSize + this->size, this->capacity*2);
        this->pData = (char*)this->pAlloc->realloc(this->pData, newCap, 1);
        this->capacity = newCap;
    }

    strncpy(this->pData + this->size, pBuff, buffSize);
    this->size += buffSize;
}

inline void
TextBuff::push(const String sBuff)
{
    this->push(sBuff.pData, sBuff.size);
}

inline void
TextBuff::reset()
{
    this->pData = {};
    this->size = {};
    this->capacity = {};
}

inline void
TextBuff::flush()
{
    if (this->size > 0)
    {
        write(STDOUT_FILENO, this->pData, this->size);
        fflush(stdout);
    }
}

inline void
TextBuff::moveTopLeft()
{
    this->push("\x1b[H");
}

inline void
TextBuff::up(int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}A", steps);
    this->push(aBuff, n);
}

inline void
TextBuff::down(int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}B", steps);
    this->push(aBuff, n);
}

inline void
TextBuff::forward(int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}C", steps);
    this->push(aBuff, n);
}

inline void
TextBuff::back(int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}D", steps);
    this->push(aBuff, n);
}

inline void
TextBuff::move(int x, int y)
{
    char aBuff[64] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{};{}H", y + 1, x + 1);
    this->push(aBuff, n);
}

inline void
TextBuff::clearDown()
{
    this->push("\x1b[0J");
}

inline void
TextBuff::clearUp()
{
    this->push("\x1b[1J");
}

inline void
TextBuff::clear()
{
    this->push("\x1b[2J");
}

inline void
TextBuff::clearLine(TEXT_BUFF_ARG eArg)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}K", int(eArg));
    this->push(aBuff, n);
}

inline void
TextBuff::moveClearLine(int x, int y, TEXT_BUFF_ARG eArg)
{
    this->move(x, y);
    this->clearLine(eArg);
}

inline void
TextBuff::hideCursor(bool bHide)
{
    if (bHide) this->push("\x1b[?25l");
    else this->push("\x1b[?25h");
}

inline void
TextBuff::movePush(int x, int y, const String str)
{
    this->move(x, y);
    this->push(str);
}

inline void
TextBuff::movePush(int x, int y, const char* pBuff, const u32 size)
{
    this->move(x, y);
    this->push(pBuff, size);
}

inline void
TextBuff::pushGlyphs(const String str, const u32 nColumns)
{
    if (!str.pData) return;

    char* pBuff = (char*)this->pAlloc->zalloc(str.size*2, 1);
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

    this->push(pBuff);
}

inline void
TextBuff::movePushGlyphs(int x, int y, const String str, const u32 nColumns)
{
    this->move(x, y);
    this->pushGlyphs(str, nColumns);
}

inline void
TextBuff::pushWideString(const wchar_t* pwBuff, const u32 wBuffSize)
{
    char* pBuff = (char*)this->pAlloc->zalloc(wBuffSize, sizeof(wchar_t));
    wcstombs(pBuff, pwBuff, wBuffSize);
    this->push(pBuff);
}

inline void
TextBuff::movePushWideString(int x, int y, const wchar_t* pwBuff, const u32 wBuffSize)
{
    this->move(x, y);
    this->pushWideString(pwBuff, wBuffSize);
}

inline void
TextBuff::clearKittyImages()
{
    this->push("\x1b_Ga=d,d=A\x1b\\");
}

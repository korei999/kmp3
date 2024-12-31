#pragma once

#include "adt/Arena.hh"
#include "adt/String.hh"
#include "adt/Vec.hh"
#include "adt/print.hh"

using namespace adt;

/* TODO: damage tracking system */

enum class TEXT_BUFF_ARG : u8
{
    TO_END, TO_BEGINNING, EVERYTHING
};

struct TextBuff
{
    Arena* m_pAlloc {};
    char* m_pData {};
    u32 m_size {};
    u32 m_capacity {};
    u32 m_tWidth {};
    u32 m_tHeight {};

    VecBase<wchar_t> m_vBackBuff {};
    VecBase<wchar_t> m_vFrontBuff {};

    /* */

    TextBuff() = default;
    TextBuff(Arena* _pAlloc) : m_pAlloc(_pAlloc) {}

    /* */

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

    void resizeBuffers(u32 width, u32 height);
    void setCell(int x, int y, wchar_t wc);
    void setString(int x, int y, const String s);
    void setWString(int x, int y, wchar_t* pBuff, const u32 buffSize);

    void swapBuffers();
    void clearBuffers();

    /* */

private:
    bool fbSet(int x, int y, wchar_t wc);
    bool bbSet(int x, int y, wchar_t wc);
};

inline void
TextBuff::push(const char* pBuff, const u32 buffSize)
{
    if (buffSize + m_size >= m_capacity)
    {
        const u32 newCap = utils::maxVal(buffSize + m_size, m_capacity*2);
        m_pData = (char*)m_pAlloc->realloc(m_pData, newCap, 1);
        m_capacity = newCap;
    }

    memcpy(m_pData + m_size, pBuff, buffSize);
    m_size += buffSize;
}

inline void
TextBuff::push(const String sBuff)
{
    push(sBuff.data(), sBuff.getSize());
}

inline void
TextBuff::reset()
{
    m_pData = {};
    m_size = {};
    m_capacity = {};
}

inline void
TextBuff::flush()
{
    if (m_size > 0)
    {
        write(STDOUT_FILENO, m_pData, m_size);
        fflush(stdout);
    }
}

inline void
TextBuff::moveTopLeft()
{
    push("\x1b[H");
}

inline void
TextBuff::up(int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}A", steps);
    push(aBuff, n);
}

inline void
TextBuff::down(int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}B", steps);
    push(aBuff, n);
}

inline void
TextBuff::forward(int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}C", steps);
    push(aBuff, n);
}

inline void
TextBuff::back(int steps)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}D", steps);
    push(aBuff, n);
}

inline void
TextBuff::move(int x, int y)
{
    char aBuff[64] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{};{}H", y + 1, x + 1);
    push(aBuff, n);
}

inline void
TextBuff::clearDown()
{
    push("\x1b[0J");
}

inline void
TextBuff::clearUp()
{
    push("\x1b[1J");
}

inline void
TextBuff::clear()
{
    push("\x1b[2J");
}

inline void
TextBuff::clearLine(TEXT_BUFF_ARG eArg)
{
    char aBuff[32] {};
    u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}K", int(eArg));
    push(aBuff, n);
}

inline void
TextBuff::moveClearLine(int x, int y, TEXT_BUFF_ARG eArg)
{
    move(x, y);
    clearLine(eArg);
}

inline void
TextBuff::hideCursor(bool bHide)
{
    if (bHide) push("\x1b[?25l");
    else push("\x1b[?25h");
}

inline void
TextBuff::movePush(int x, int y, const String str)
{
    move(x, y);
    push(str);
}

inline void
TextBuff::movePush(int x, int y, const char* pBuff, const u32 size)
{
    move(x, y);
    push(pBuff, size);
}

inline void
TextBuff::pushGlyphs(const String str, const u32 nColumns)
{
    if (!str.data()) return;

    char* pBuff = (char*)m_pAlloc->zalloc(str.getSize()*2, 1);
    u32 off = 0;

    u32 totalWidth = 0;
    for (u32 i = 0; i < str.getSize() && totalWidth < nColumns; )
    {
        wchar_t wc {};
        int wclen = mbtowc(&wc, &str[i], str.getSize() - i);
        totalWidth += wcwidth(wc);
        i += wclen;

        off += wctomb(pBuff + off, wc);
    }

    push(pBuff);
}

inline void
TextBuff::movePushGlyphs(int x, int y, const String str, const u32 nColumns)
{
    move(x, y);
    pushGlyphs(str, nColumns);
}

inline void
TextBuff::pushWideString(const wchar_t* pwBuff, const u32 wBuffSize)
{
    char* pBuff = (char*)m_pAlloc->zalloc(wBuffSize, sizeof(wchar_t));
    wcstombs(pBuff, pwBuff, wBuffSize);
    push(pBuff);
}

inline void
TextBuff::movePushWideString(int x, int y, const wchar_t* pwBuff, const u32 wBuffSize)
{
    move(x, y);
    pushWideString(pwBuff, wBuffSize);
}

inline void
TextBuff::clearKittyImages()
{
    push("\x1b_Ga=d,d=A\x1b\\");
}

inline bool
TextBuff::fbSet(int x, int y, wchar_t wc)
{
    if (m_tWidth*y + x < m_vFrontBuff.getSize())
    {
        m_vFrontBuff[m_tWidth*y + x] = wc;
        return true;
    }

    return false;
}

inline bool
TextBuff::bbSet(int x, int y, wchar_t wc)
{
    if (m_tWidth*y + x < m_vBackBuff.getSize())
    {
        m_vBackBuff[m_tWidth*y + x] = wc;
        return true;
    }

    return false;
}

inline void
TextBuff::resizeBuffers(u32 width, u32 height)
{
    m_tWidth = width, m_tHeight = height;
    m_vFrontBuff.setSize(OsAllocatorGet(), width * height);
    m_vBackBuff.setSize(OsAllocatorGet(), width * height);
}

inline void
TextBuff::setCell(int x, int y, wchar_t wc)
{
    fbSet(x, y, wc);
}

inline void
TextBuff::setString(int x, int y, const String s)
{
    int xOff = 0;
    for (auto wch : StringGlyphIt(s))
    {
        int cols = wcswidth(&wch, sizeof(wch));
        if (cols < 0) return;

        xOff += cols;
        setCell(x + xOff, y, wch);
    }
}

inline void
TextBuff::setWString(int x, int y, wchar_t* pBuff, const u32 buffSize)
{
    /* TODO: */
}

inline void
TextBuff::swapBuffers()
{
    utils::swap(&m_vBackBuff, &m_vFrontBuff);
    m_vFrontBuff.zeroOut();
}

inline void
TextBuff::clearBuffers()
{
    m_vBackBuff.zeroOut();
    m_vFrontBuff.zeroOut();
}

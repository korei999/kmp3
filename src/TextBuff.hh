#pragma once

#include "adt/Arena.hh"
#include "adt/String.hh"
#include "adt/print.hh"
#include "adt/enum.hh"

#include <threads.h>

using namespace adt;

enum class TEXT_BUFF_ARG : u8
{
    TO_END, TO_BEGINNING, EVERYTHING
};

enum class TEXT_BUFF_STYLE : u32
{
    NORM = 0,
    BOLD = 1,
    ITALIC = 1 << 1,
    REVERSE = 1 << 2,

    RED = 1 << 3,
    GREEN = 1 << 4,
    YELLOW = 1 << 5,
    BLUE = 1 << 6,
    MAGENTA = 1 << 7,
    CYAN = 1 << 8,
    WHITE = 1 << 9,

    BG_RED = 1 << 10,
    BG_GREEN = 1 << 11,
    BG_YELLOW = 1 << 12,
    BG_BLUE = 1 << 13,
    BG_MAGENTA = 1 << 14,
    BG_CYAN = 1 << 15,
    BG_WHITE = 1 << 16,
};
ADT_ENUM_BITWISE_OPERATORS(TEXT_BUFF_STYLE);

struct TextBuffCell
{
    wchar_t wc {};
    // TEXT_BUFF_STYLE eStyle {};
};

inline bool
operator==(const TextBuffCell l, const TextBuffCell r)
{
    return l.wc == r.wc; // && l.eStyle == r.eStyle;
}

inline bool
operator!=(const TextBuffCell l, const TextBuffCell r)
{
    return !(l == r);
}

struct TextBuff
{
    Arena* m_pAlloc {};
    mtx_t m_mtx {};
    char* m_pData {};
    u32 m_size {};
    u32 m_capacity {};
    u32 m_tWidth {};
    u32 m_tHeight {};
    bool m_bChanged {};

    /*VecBase<TextBuffCell> m_vBackBuff {};*/
    /*VecBase<TextBuffCell> m_vFrontBuff {};*/

    /* */

    TextBuff() = default;
    TextBuff(Arena* _pAlloc) : m_pAlloc(_pAlloc) { mtx_init(&m_mtx, mtx_plain); }

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
    void pushWString(const wchar_t* pwBuff, const u32 wBuffSize);
    int pushWChar(wchar_t wc);
    void movePushWString(int x, int y, const wchar_t* pwBuff, const u32 wBuffSize);
    void clearKittyImages();

    void resizeBuffers(u32 width, u32 height);
    bool setCell(int x, int y, wchar_t wc);
    void setString(int x, int y, const String s);
    void setWString(int x, int y, wchar_t* pBuff, const u32 buffSize);

    void swapBuffers();
    void clearBuffers();
    void clearFrontBuffer();
    void present();
    void destroy();

    /* */

private:
    bool fbSet(int x, int y, wchar_t wc);
    bool bbSet(int x, int y, wchar_t wc);
    void pushDiff();
};

inline void
TextBuff::push(const char* pBuff, const u32 buffSize)
{
    if (buffSize + m_size >= m_capacity)
    {
        const u32 newCap = utils::max(buffSize + m_size, m_capacity*2);
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
TextBuff::pushWString(const wchar_t* pwBuff, const u32 wBuffSize)
{
    char* pBuff = (char*)m_pAlloc->zalloc(wBuffSize, sizeof(wchar_t));
    wcstombs(pBuff, pwBuff, wBuffSize);
    push(pBuff);
}

inline int
TextBuff::pushWChar(wchar_t wc)
{
    char a[10] {};
    int len = wctomb(a, wc);
    push(a, len);

    return len;
}

inline void
TextBuff::movePushWString(int x, int y, const wchar_t* pwBuff, const u32 wBuffSize)
{
    move(x, y);
    pushWString(pwBuff, wBuffSize);
}

inline void
TextBuff::clearKittyImages()
{
    push("\x1b_Ga=d,d=A\x1b\\");
}

//inline bool
//TextBuff::fbSet(int x, int y, wchar_t wc)
//{
//    if (m_tWidth*y + x < m_vFrontBuff.getSize())
//    {
//        m_vFrontBuff[m_tWidth*y + x].wc = wc;
//        m_bChanged = true;
//        return true;
//    }
//
//    return false;
//}

//inline bool
//TextBuff::bbSet(int x, int y, wchar_t wc)
//{
//    if (m_tWidth*y + x < m_vBackBuff.getSize())
//    {
//        m_vBackBuff[m_tWidth*y + x].wc = wc;
//        return true;
//    }
//
//    return false;
//}

inline void
TextBuff::resizeBuffers(u32 width, u32 height)
{
    m_tWidth = width, m_tHeight = height;
//    m_vFrontBuff.setSize(OsAllocatorGet(), width * height);
//    m_vBackBuff.setSize(OsAllocatorGet(), width * height);
}

inline bool
TextBuff::setCell(int x, int y, wchar_t wc)
{
    return fbSet(x, y, wc);
}

inline void
TextBuff::setString(int x, int y, const String s)
{
    u32 xOff = 0;
    for (auto wch : StringGlyphIt(s))
        if (!setCell(x + (xOff++), y, wch)) return;
}

inline void
TextBuff::setWString(int x, int y, wchar_t* pBuff, const u32 buffSize)
{
    for (u32 i = 0; i < buffSize; ++i)
        if (!setCell(x, y, pBuff[i])) return;
}

inline void
TextBuff::swapBuffers()
{
    // utils::swap(&m_vBackBuff, &m_vFrontBuff);
}

inline void
TextBuff::clearBuffers()
{
    /*m_vBackBuff.zeroOut();*/
    /*m_vFrontBuff.zeroOut();*/

//    utils::fill(m_vFrontBuff.data(), {L' '}, m_vFrontBuff.getSize());
//    utils::fill(m_vBackBuff.data(), {L' '}, m_vBackBuff.getSize());
}

inline void
TextBuff::clearFrontBuffer()
{
    // utils::fill(m_vFrontBuff.data(), {L' '}, m_vFrontBuff.getSize());
}

inline void
TextBuff::present()
{
    if (!m_bChanged) return;

    pushDiff();
    m_bChanged = false;
}

inline void
TextBuff::destroy()
{
    // m_vBackBuff.destroy(OsAllocatorGet());
    // m_vFrontBuff.destroy(OsAllocatorGet());
    mtx_destroy(&m_mtx);
}

inline void
TextBuff::pushDiff()
{
//    auto& bb = m_vBackBuff;
//    auto& fb = m_vFrontBuff;
//
//    for (u32 h = 0; h < m_tHeight; ++h)
//    {
//        u32 xOff = 0;
//
//        for (u32 w = 0; w < m_tWidth; ++w)
//        {
//            const u32 pos = m_tWidth*h + w;
//        }
//    }
}

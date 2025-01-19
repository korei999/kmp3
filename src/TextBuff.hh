#pragma once

#include "adt/Arena.hh"
#include "adt/String.hh"
#include "adt/Vec.hh"
#include "adt/print.hh"
#include "adt/Span2D.hh"
#include "adt/enum.hh"

#include "adt/logs.hh"

#include <threads.h>

#define TEXT_BUFF_MOUSE_ENABLE "\x1b[?1000h\x1b[?1002h\x1b[?1015h\x1b[?1006h"
#define TEXT_BUFF_MOUSE_DISABLE "\x1b[?1006l\x1b[?1015l\x1b[?1002l\x1b[?1000l"
#define TEXT_BUFF_KEYPAD_ENABLE "\033[?1h\033="
#define TEXT_BUFF_KEYPAD_DISABLE "\033[?1l\033>"

#define TEXT_BUFF_NORM "\x1b[0m"
#define TEXT_BUFF_BOLD "\x1b[1m"
#define TEXT_BUFF_ITALIC "\x1b[3m"
#define TEXT_BUFF_REVERSE "\x1b[7m"

#define TEXT_BUFF_RED "\x1b[31m"
#define TEXT_BUFF_GREEN "\x1b[32m"
#define TEXT_BUFF_YELLOW "\x1b[33m"
#define TEXT_BUFF_BLUE "\x1b[34m"
#define TEXT_BUFF_MAGENTA "\x1b[35m"
#define TEXT_BUFF_CYAN "\x1b[36m"
#define TEXT_BUFF_WHITE "\x1b[37m"

#define TEXT_BUFF_BG_RED "\x1b[41m"
#define TEXT_BUFF_BG_GREEN "\x1b[42m"
#define TEXT_BUFF_BG_YELLOW "\x1b[43m"
#define TEXT_BUFF_BG_BLUE "\x1b[44m"
#define TEXT_BUFF_BG_MAGENTA "\x1b[45m"
#define TEXT_BUFF_BG_CYAN "\x1b[46m"
#define TEXT_BUFF_BG_WHITE "\x1b[47m"

using namespace adt;

enum class TEXT_BUFF_ARG : u8
{
    TO_END, TO_BEGINNING, EVERYTHING
};

enum TEXT_BUFF_STYLE : u32
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

    IMAGE = 1 << 17,

    FORCE_REDRAW = 1 << 18,
};
ADT_ENUM_BITWISE_OPERATORS(TEXT_BUFF_STYLE);

struct TextBuffCell
{
    wchar_t wc {};
    TEXT_BUFF_STYLE eStyle {};
};

inline bool
operator==(const TextBuffCell& l, const TextBuffCell& r)
{
    return (l.wc == r.wc) && (l.eStyle == r.eStyle);
}

inline bool
operator!=(const TextBuffCell& l, const TextBuffCell& r)
{
    return !(l == r);
}

struct TextBuff
{
    Arena* m_pAlloc {};

    mtx_t m_mtx {};

    char* m_pData {};
    ssize m_size {};
    ssize m_capacity {};

    ssize m_tWidth {};
    ssize m_tHeight {};

    bool m_bChanged {};

    VecBase<TextBuffCell> m_vFront {}; /* what to show */
    VecBase<TextBuffCell> m_vBack {}; /* where to draw */

    /* */

    TextBuff() = default;
    TextBuff(Arena* _pAlloc) : m_pAlloc(_pAlloc) { mtx_init(&m_mtx, mtx_plain); }

    /* direct write api (slow) */
    void push(const char* pBuff, const ssize buffSize);
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
    void movePush(int x, int y, const char* pBuff, const ssize size);
    void pushGlyph(wchar_t wc);
    void pushGlyphs(const String str, const ssize nColumns);
    void movePushGlyphs(int x, int y, const String str, const ssize nColumns);
    void pushWString(const wchar_t* pwBuff, const ssize wBuffSize);
    int pushWChar(wchar_t wc);
    void movePushWString(int x, int y, const wchar_t* pwBuff, const ssize wBuffSize);
    void clearKittyImages();
    void resizeBuffers(ssize width, ssize height);
    void destroy();
    /* */

    /* new api (less slow) */
    void init(ssize termWidth, ssize termHeight);
    void swapBuffers();
    void clearBackBuffer();
    void resetBuffers();

    void string(int x, int y, TEXT_BUFF_STYLE eStyle, const String str);
    void wideString(int x, int y, TEXT_BUFF_STYLE eStyle, Span<wchar_t> sp);
    String styleToString(TEXT_BUFF_STYLE eStyle);
    void image(int x, int y, int width, int height, const String str);
    void clearImage(int x, int y, int width, int height);
    /* */

private:
    Span2D<TextBuffCell> frontBufferSpan();
    Span2D<TextBuffCell> backBufferSpan();
};

inline void
TextBuff::push(const char* pBuff, const ssize buffSize)
{
    if (buffSize + m_size >= m_capacity)
    {
        const ssize newCap = utils::max(buffSize + m_size, m_capacity*2);
        m_pData = (char*)m_pAlloc->realloc(m_pData, m_capacity, newCap, 1);
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
    LOG_GOOD("flush(): m_size: {}\n", m_size);
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
    ssize n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}A", steps);
    push(aBuff, n);
}

inline void
TextBuff::down(int steps)
{
    char aBuff[32] {};
    ssize n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}B", steps);
    push(aBuff, n);
}

inline void
TextBuff::forward(int steps)
{
    char aBuff[32] {};
    ssize n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}C", steps);
    push(aBuff, n);
}

inline void
TextBuff::back(int steps)
{
    char aBuff[32] {};
    ssize n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}D", steps);
    push(aBuff, n);
}

inline void
TextBuff::move(int x, int y)
{
    char aBuff[32] {};
    ssize n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{};{}H", y + 1, x + 1);
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
    ssize n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[{}K", int(eArg));
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
TextBuff::movePush(int x, int y, const char* pBuff, const ssize size)
{
    move(x, y);
    push(pBuff, size);
}

inline void
TextBuff::pushGlyph(wchar_t wc)
{
    char aBuff[8] {};
    int len = wctomb(aBuff, wc);
    push(aBuff, len);
}

inline void
TextBuff::pushGlyphs(const String str, const ssize nColumns)
{
    if (!str.data())
        return;

    char* pBuff = (char*)m_pAlloc->zalloc(str.getSize()*2, 1);
    ssize off = 0;

    ssize totalWidth = 0;
    for (ssize i = 0; i < str.getSize() && totalWidth < nColumns; )
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
TextBuff::movePushGlyphs(int x, int y, const String str, const ssize nColumns)
{
    move(x, y);
    pushGlyphs(str, nColumns);
}

inline void
TextBuff::pushWString(const wchar_t* pwBuff, const ssize wBuffSize)
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
TextBuff::movePushWString(int x, int y, const wchar_t* pwBuff, const ssize wBuffSize)
{
    move(x, y);
    pushWString(pwBuff, wBuffSize);
}

inline void
TextBuff::clearKittyImages()
{
    push("\x1b_Ga=d,d=A\x1b\\");
}

inline void
TextBuff::resizeBuffers(ssize width, ssize height)
{
    m_tWidth = width, m_tHeight = height;
    m_vBack.setSize(OsAllocatorGet(), width * height);
    m_vFront.setSize(OsAllocatorGet(), width * height);

    resetBuffers();
}

inline void
TextBuff::destroy()
{
    mtx_destroy(&m_mtx);
    m_vBack.destroy(OsAllocatorGet());
    m_vFront.destroy(OsAllocatorGet());
}

inline void
TextBuff::init(ssize termWidth, ssize termHeight)
{
    clear();
    hideCursor(true);
    push(TEXT_BUFF_KEYPAD_ENABLE);
    flush();

    resizeBuffers(termWidth, termHeight);
}

inline void
TextBuff::swapBuffers()
{
    if (!m_bChanged)
        return;

    m_bChanged = false;

    ssize row = 0;
    ssize col = 0;
    ssize lastCol = 0;
    bool bResetStyle = true;

    TEXT_BUFF_STYLE eLastStyle = TEXT_BUFF_STYLE::NORM;

    for (row = 0; row < m_tHeight; ++row)
    {
        lastCol = -1; /* fix for col == 0 */
        col = 0;
        move(col, row);

        for (; col < m_tWidth; ++col)
        {
            auto& front = frontBufferSpan()(col, row);
            auto& back = backBufferSpan()(col, row);

            if (front != back)
            {
                bResetStyle = true;

                if (lastCol + 1 != col)
                {
                    move(col, row);
                    lastCol = col;
                }

                if (back.eStyle != eLastStyle)
                {
                    push(styleToString(back.eStyle));
                    eLastStyle = back.eStyle;
                }

                pushGlyph(back.wc);

                int colWidth = wcwidth(back.wc);
                if (colWidth > 1)
                    col += colWidth - 1;
            }
            else
            {
                if (bResetStyle)
                {
                    bResetStyle = false;
                    eLastStyle = TEXT_BUFF_STYLE::NORM;
                    push(TEXT_BUFF_NORM);
                }
            }
        }
    }

    utils::swap(&m_vFront, &m_vBack);
}

inline void
TextBuff::clearBackBuffer()
{
    for (auto& cell : m_vBack)
    {
        if (cell.eStyle != TEXT_BUFF_STYLE::IMAGE)
        {
            cell.wc = L' ';
            cell.eStyle = TEXT_BUFF_STYLE::NORM;
        }
    }
}

inline void
TextBuff::resetBuffers()
{
    for (auto& cell : m_vFront)
    {
        cell.wc = L' ';
        cell.eStyle = TEXT_BUFF_STYLE::NORM;
    }
    for (auto& cell : m_vFront)
    {
        cell.wc = L' ';
        cell.eStyle = TEXT_BUFF_STYLE::NORM;
    }

    clear();
}

inline Span2D<TextBuffCell>
TextBuff::frontBufferSpan()
{
    return {m_vFront.data(), m_tWidth, m_tHeight};
}

inline Span2D<TextBuffCell>
TextBuff::backBufferSpan()
{
    return {m_vBack.data(), m_tWidth, m_tHeight};
}

inline void
TextBuff::string(int x, int y, TEXT_BUFF_STYLE eStyle, const String str)
{
    if (x < 0 || x >= m_tWidth || y < 0 || y >= m_tHeight)
        return;

    Span2D bb = backBufferSpan();
    Span2D fb = frontBufferSpan();

    for (wchar_t wc : StringGlyphIt(str))
    {
        if (x >= m_tWidth)
            break;

        if (fb(x, y) != TextBuffCell{wc, eStyle})
            m_bChanged = true;

        bb(x, y).wc = wc;
        bb(x, y).eStyle = eStyle;

        int colWidth = wcwidth(wc);
        if (colWidth > 1)
        {
            for (ssize i = 1; i < colWidth; ++i)
            {
                auto& front = fb(x + i, y);
                auto& back = bb(x + i, y);

                if (front.wc != wc || front.eStyle != eStyle)
                {
                    back.wc = L' ';
                    back.eStyle = eStyle;
                }
            }
        }

        if (colWidth == 0)
            break;

        x += colWidth;
    }
}

inline void
TextBuff::wideString(int x, int y, TEXT_BUFF_STYLE eStyle, Span<wchar_t> sp)
{
    if (x < 0 || x >= m_tWidth || y < 0 || y >= m_tHeight)
        return;

    auto* pBuff = (char*)m_pAlloc->zalloc(sp.getSize() * 8, sizeof(*sp.data()));
    int len = wcstombs(pBuff, sp.data(), sp.getSize());
    string(x, y, eStyle, {pBuff, len});
}

inline String
TextBuff::styleToString(TEXT_BUFF_STYLE eStyle)
{
    const ssize size = 511; /* big enough */
    char* pBuff = (char*)m_pAlloc->zalloc(1, size + 1);
    ssize n = 0;

    n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_NORM);

    if (eStyle == 0)
        return {pBuff, n};

    if (eStyle & BOLD)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_BOLD);
    if (eStyle & ITALIC)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_ITALIC);
    if (eStyle & REVERSE)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_REVERSE);
    if (eStyle & RED)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_RED);
    if (eStyle & GREEN)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_GREEN);
    if (eStyle & YELLOW)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_YELLOW);
    if (eStyle & BLUE)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_BLUE);
    if (eStyle & MAGENTA)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_MAGENTA);
    if (eStyle & CYAN)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_CYAN);
    if (eStyle & WHITE)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_WHITE);
    if (eStyle & BG_RED)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_BG_RED);
    if (eStyle & BG_GREEN)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_BG_GREEN);
    if (eStyle & BG_YELLOW)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_BG_YELLOW);
    if (eStyle & BG_BLUE)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_BG_BLUE);
    if (eStyle & BG_MAGENTA)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_BG_MAGENTA);
    if (eStyle & BG_CYAN)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_BG_CYAN);
    if (eStyle & BG_WHITE)
        n += print::toBuffer(pBuff + n, size - n, TEXT_BUFF_BG_WHITE);

    return {pBuff, n};
}

inline void
TextBuff::image(int x, int y, int width, int height, const String str)
{
    if (x < 0 || x >= m_tWidth || y < 0 || y >= m_tHeight)
        return;

    width = utils::min((ssize)width, m_tWidth);
    height = utils::min((ssize)height, m_tHeight);

    for (ssize row = y; row < height; ++row)
    {
        for (ssize col = x; col < width; ++col)
        {
            auto& back = backBufferSpan()(col, row);
            back.eStyle = IMAGE;
            back.wc = L' ';
        }
    }

    movePush(x, y, str);
}

inline void
TextBuff::clearImage(int x, int y, int width, int height)
{
    if (x < 0 || x >= m_tWidth || y < 0 || y >= m_tHeight)
        return;

    width = utils::min((ssize)width, m_tWidth);
    height = utils::min((ssize)height, m_tHeight);

    for (ssize row = y; row < height; ++row)
    {
        for (ssize col = x; col < width; ++col)
        {
            auto& back = backBufferSpan()(col, row);
            back.eStyle = NORM;
            back.wc = L' ';
        }
    }
}

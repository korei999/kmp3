#include "TextBuff.hh"

#include "adt/logs.hh"
#include "adt/print.hh"

#include <cwctype>

#define TEXT_BUFF_MOUSE_ENABLE "\x1b[?1000h\x1b[?1002h\x1b[?1015h\x1b[?1006h"
#define TEXT_BUFF_MOUSE_DISABLE "\x1b[?1006l\x1b[?1015l\x1b[?1002l\x1b[?1000l"
#define TEXT_BUFF_KEYPAD_ENABLE "\x1b[?1h\033="
#define TEXT_BUFF_KEYPAD_DISABLE "\x1b[?1l\033>"
#define TEXT_BUFF_ALT_SCREEN_ENABLE "\x1b[?1049h"
#define TEXT_BUFF_ALT_SCREEN_DISABLE "\x1b[?1049l"

#define TEXT_BUFF_NORM "\x1b[0m"
#define TEXT_BUFF_BOLD "\x1b[1m"
#define TEXT_BUFF_DIM "\x1b[2m"
#define TEXT_BUFF_ITALIC "\x1b[3m"
#define TEXT_BUFF_UNDERLINE "\x1b[4m"
#define TEXT_BUFF_BLINK "\x1b[5m"
#define TEXT_BUFF_REVERSE "\x1b[7m"
#define TEXT_BUFF_INVIS "\x1b[8m"
#define TEXT_BUFF_STRIKE "\x1b[9m"

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

namespace platform::ansi
{

void
TextBuff::grow(ssize newCap)
{
    m_pData = m_pArena->reallocV<char>(m_pData, m_capacity, newCap);
    m_capacity = newCap;
}

void
TextBuff::push(const char ch)
{
    if (m_size >= m_capacity)
    {
        const ssize newCap = utils::max(ssize(2), m_capacity*2);
        grow(newCap);
    }

    m_pData[m_size++] = ch;
}

void
TextBuff::push(const char* pBuff, const ssize buffSize)
{
    if (buffSize + m_size >= m_capacity)
    {
        const ssize newCap = utils::max(buffSize + m_size, m_capacity*2);
        grow(newCap);
    }

    memcpy(m_pData + m_size, pBuff, buffSize);
    m_size += buffSize;
}

void
TextBuff::push(const StringView sv)
{
    push(sv.data(), sv.size());
}

void
TextBuff::reset()
{
    m_pData = {};
    m_size = {};
    m_capacity = {};
    m_scratch = {};
}

void
TextBuff::flush()
{
    if (m_size > 0)
    {
        [[maybe_unused]] auto _unused =
            write(STDOUT_FILENO, m_pData, m_size);

        LOG_WARN("\nbytes: {}\n", _unused);

        m_size = 0;
    }
}

void
TextBuff::moveTopLeft()
{
    push("\x1b[H");
}

void
TextBuff::up(int steps)
{
    char aBuff[32] {};
    ssize n = print::toSpan(aBuff, "\x1b[{}A", steps);
    push(aBuff, n);
}

void
TextBuff::down(int steps)
{
    char aBuff[32] {};
    ssize n = print::toSpan(aBuff, "\x1b[{}B", steps);
    push(aBuff, n);
}

void
TextBuff::forward(int steps)
{
    char aBuff[32] {};
    ssize n = print::toSpan(aBuff, "\x1b[{}C", steps);
    push(aBuff, n);
}

void
TextBuff::back(int steps)
{
    char aBuff[32] {};
    ssize n = print::toSpan(aBuff, "\x1b[{}D", steps);
    push(aBuff, n);
}

void
TextBuff::move(int x, int y)
{
    char aBuff[32] {};
    ssize n = print::toSpan(aBuff, "\x1b[{};{}H", y + 1, x + 1);
    push(aBuff, n);
}

void
TextBuff::clearTermDown()
{
    push("\x1b[0J");
}

void
TextBuff::clearTermUp()
{
    push("\x1b[1J");
}

void
TextBuff::clearTerm()
{
    push("\x1b[2J");
}

void
TextBuff::clearLine(TEXT_BUFF_ARG eArg)
{
    char aBuff[32] {};
    ssize n = print::toSpan(aBuff, "\x1b[{}K", int(eArg));
    push(aBuff, n);
}

void
TextBuff::hideCursor(bool bHide)
{
    if (bHide) push("\x1b[?25l");
    else push("\x1b[?25h");
}

void
TextBuff::pushGlyph(wchar_t wc)
{
    char aBuff[8] {};
    int len = wctomb(aBuff, wc);

    if (len > 1) push(aBuff, len);
    else push(aBuff[0]);
}

void
TextBuff::clearKittyImages()
{
    push("\x1b_Ga=d,d=A\x1b\\");
}

void
TextBuff::resizeBuffers(ssize width, ssize height)
{
    m_vBack.setSize(StdAllocator::inst(), width * height);
    m_vFront.setSize(StdAllocator::inst(), width * height);

    m_tWidth = width;
    m_tHeight = height;

    resetBuffers();
}

void
TextBuff::erase()
{
    m_bErase = true;
}

void
TextBuff::resize(ssize width, ssize height)
{
    m_bResize = true;
    m_newTWidth = width;
    m_newTHeight = height;
}

void
TextBuff::destroy()
{
    m_vBack.destroy(StdAllocator::inst());
    m_vFront.destroy(StdAllocator::inst());

#ifdef OPT_CHAFA
    m_imgArena.freeAll();
#endif

    hideCursor(false);
    clearKittyImages();
    push(TEXT_BUFF_KEYPAD_DISABLE);
    push(TEXT_BUFF_ALT_SCREEN_DISABLE);
    push(TEXT_BUFF_MOUSE_DISABLE);
    flush();
}

void
TextBuff::start(Arena* pArena, ssize termWidth, ssize termHeight)
{
    m_pArena = pArena;
#ifdef OPT_CHAFA
    m_imgArena = Arena(SIZE_1M);
#endif

    clearTerm();
    moveTopLeft();
    push(TEXT_BUFF_ALT_SCREEN_ENABLE);
    push(TEXT_BUFF_KEYPAD_ENABLE);
    push(TEXT_BUFF_MOUSE_ENABLE);
    hideCursor(true);
    flush();

    resizeBuffers(termWidth, termHeight);
}

void
TextBuff::pushDiff()
{
    ssize row = 0;
    ssize col = 0;
    ssize nForwards = 0;
    TEXT_BUFF_STYLE eLastStyle = TEXT_BUFF_STYLE::NORM;
    bool bChangeStyle = false;

    for (row = 0; row < m_tHeight; ++row)
    {
        nForwards = 0;
        bool bMoved = false;

        auto clMove = [&]
        {
            if (!bMoved)
            {
                move(0, row);
                bMoved = true;
            }
        };

        for (col = 0; col < m_tWidth; ++col)
        {
            const auto& front = frontBufferSpan()(col, row);
            const auto& back = backBufferSpan()(col, row);

            if (back.eStyle != eLastStyle)
                bChangeStyle = true;

            if (front != back)
            {
                if (nForwards > 0)
                {
                    clMove();
                    forward(nForwards);
                    nForwards = 0;
                }

                int colWidth = wcwidth(back.wc);

                if (col + colWidth <= m_tWidth)
                {
                    clMove();
                    if (bChangeStyle)
                    {
                        bChangeStyle = false;
                        push(styleToStringScratch(back.eStyle));
                        eLastStyle = back.eStyle;
                    }
                    pushGlyph(back.wc);
                }

                if (colWidth > 1) col += colWidth - 1;
            }
            else
            {
                ++nForwards;
            }
        }
    }
}

#ifdef OPT_CHAFA
void
TextBuff::showImages()
{
    int nDraws = 0;

    for (const auto& im : m_vImages)
    {
        ++nDraws;
        if (im.img.eLayout == platform::chafa::IMAGE_LAYOUT::RAW)
        {
            move(im.x, im.y);
            push(im.img.uData.sRaw);
        }
        else
        {
            auto& vLines = im.img.uData.vLines;
            for (ssize lineIdx = 0; lineIdx < vLines.size(); ++lineIdx)
            {
                move(im.x, im.y + lineIdx);
                push(vLines[lineIdx]);
            }
        }
    }

    if (nDraws > 0)
    {
        m_vImages.destroy(&m_imgArena);
        m_imgArena.reset();
    }
}
#endif

void
TextBuff::clearBackBuffer()
{
    for (auto& cell : m_vBack)
    {
        cell.wc = L' ';
        cell.eStyle = TEXT_BUFF_STYLE::NORM;
    }
}

void
TextBuff::resetBuffers()
{
    for (auto& cell : m_vFront)
    {
        cell.wc = L' ';
        cell.eStyle = TEXT_BUFF_STYLE::NORM;
    }

    utils::memCopy(m_vBack.data(), m_vFront.data(), m_vFront.size());
}

void
TextBuff::clean()
{
    if (m_bErase)
    {
        m_bErase = false;
        clearTerm();
        resetBuffers();
        flush();
    }

    if (m_bResize)
    {
        m_bResize = false;
        resizeBuffers(m_newTWidth, m_newTHeight);
    }
    else
    {
        clearBackBuffer();
    }

    ssize scratchSize = SIZE_1K;
    m_scratch = Span((char*)m_pArena->malloc(scratchSize, 1), scratchSize);
}

void
TextBuff::present()
{
    if (m_bErase)
    {
        m_bErase = false;
        clearTerm();
    }

    if (m_bChanged)
    {
        m_bChanged = false;
        pushDiff();

#ifdef OPT_CHAFA
        showImages();
#endif

        utils::swap(&m_vFront, &m_vBack);
    }

    flush();
    reset();
}

Span2D<TextBuffCell>
TextBuff::frontBufferSpan()
{
    return {
        m_vFront.data(),
        static_cast<int>(m_tWidth),
        static_cast<int>(m_tHeight),
        static_cast<int>(m_tWidth)
    };
}

Span2D<TextBuffCell>
TextBuff::backBufferSpan()
{
    return {
        m_vBack.data(),
        static_cast<int>(m_tWidth),
        static_cast<int>(m_tHeight),
        static_cast<int>(m_tWidth)
    };
}

void
TextBuff::string(int x, int y, TEXT_BUFF_STYLE eStyle, const StringView str, int maxSvLen)
{
    if (x < 0 || x >= m_tWidth || y < 0 || y >= m_tHeight)
        return;

    Span2D bb = backBufferSpan();
    Span2D fb = frontBufferSpan();

    int max = 0;
    for (const auto& wc : StringGlyphIt(str))
    {
        if (x >= m_tWidth || max >= maxSvLen) break;

        if (fb(x, y) != TextBuffCell{wc, eStyle})
            m_bChanged = true;

        bb(x, y).wc = wc;
        bb(x, y).eStyle = eStyle;

        int colWidth = wcwidth(wc);
        if (colWidth > 1)
        {
            for (ssize i = 1; i < colWidth; ++i)
            {
                if (x + i >= bb.width() || y >= bb.height())
                    return;

                auto& back = bb(x + i, y);
                back.wc = L' ';
                back.eStyle = eStyle;
            }
        }

        x += colWidth;
        max += colWidth;
    }
}

void
TextBuff::wideString(int x, int y, TEXT_BUFF_STYLE eStyle, Span<wchar_t> sp)
{
    if (x < 0 || x >= m_tWidth || y < 0 || y >= m_tHeight)
        return;

    Span spBuff = m_scratch.nextMemZero<char>(sp.size() * 8);

    if (spBuff.size() > 0)
    {
        int len = wcstombs(spBuff.data(), sp.data(), spBuff.size() - 1);
        string(x, y, eStyle, {spBuff.data(), len});
    }
}

StringView
TextBuff::styleToStringScratch(TEXT_BUFF_STYLE eStyle)
{
    Span<char> sp = m_scratch.nextMemZero<char>(128);
    if (sp.size() <= 0) return {};

    ssize size = sp.size() - 1;
    ssize n = 0;

    n += print::toBuffer(sp.data() + n, size - n, "\x1b[0");

    if (bool(eStyle))
    {
        using CODE = TEXT_BUFF_STYLE_CODE;
        using STYLE = TEXT_BUFF_STYLE;

        if (bool(eStyle & STYLE::BOLD))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::BOLD));
        if (bool(eStyle & STYLE::DIM))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::DIM));
        if (bool(eStyle & STYLE::ITALIC))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::ITALIC));
        if (bool(eStyle & STYLE::UNDERLINE))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::UNRELINE));
        if (bool(eStyle & STYLE::BLINK))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::BLINK));
        if (bool(eStyle & STYLE::REVERSE))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::REVERSE));
        if (bool(eStyle & STYLE::INVIS))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::INVIS));
        if (bool(eStyle & STYLE::STRIKE))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::STRIKE));
        if (bool(eStyle & STYLE::RED))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::RED));
        if (bool(eStyle & STYLE::GREEN))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::GREEN));
        if (bool(eStyle & STYLE::YELLOW))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::YELLOW));
        if (bool(eStyle & STYLE::BLUE))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::BLUE));
        if (bool(eStyle & STYLE::MAGENTA))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::MAGENTA));
        if (bool(eStyle & STYLE::CYAN))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::CYAN));
        if (bool(eStyle & STYLE::WHITE))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::WHITE));
        if (bool(eStyle & STYLE::BG_RED))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::BG_RED));
        if (bool(eStyle & STYLE::BG_GREEN))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::BG_GREEN));
        if (bool(eStyle & STYLE::BG_YELLOW))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::BG_YELLOW));
        if (bool(eStyle & STYLE::BG_BLUE))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::BG_BLUE));
        if (bool(eStyle & STYLE::BG_MAGENTA))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::BG_MAGENTA));
        if (bool(eStyle & STYLE::BG_CYAN))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::BG_CYAN));
        if (bool(eStyle & STYLE::BG_WHITE))
            n += print::toBuffer(sp.data() + n, size - n, ";{}", int(CODE::BG_WHITE));
    }

    n += print::toBuffer(sp.data() + n, size - n, "m");

    return {sp.data(), n};
}

#ifdef OPT_CHAFA
void
TextBuff::image(int x, int y, const platform::chafa::Image& img)
{
    if (img.width <= 0 || img.height <= 0) return;
    m_vImages.push(&m_imgArena, {img, x, y});
}

void
TextBuff::forceClean(int x, int y, int width, int height)
{
    if (x < 0 || y < 0)
        return;

    width = utils::min((ssize)width, m_tWidth);
    height = utils::min((ssize)height, m_tHeight);

    auto spFront = frontBufferSpan();
    auto spBack = backBufferSpan();

    for (ssize row = y; row < height; ++row)
    {
        for (ssize col = x; col < width; ++col)
        {
            auto& front = spFront(col, row);
            auto& back = spBack(col, row);

            /* trigger diff */
            front.wc = 666;
            back.wc = L' ';
        }
    }
}
#endif

} /* namespace platform::ansi */

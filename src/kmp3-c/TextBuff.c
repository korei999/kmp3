#include "TextBuff.h"

#include "klib/Gpa.h"
#include "klib/Span.h"
#include "klib/assert.h"
#include "klib/print.h"
#include "klib/utf8.h"

#include <unistd.h>

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

static bool TextBuffCellEq(TextBuffCell* pLeft, TextBuffCell* pRight);

static ssize_t TextBuffStyleToBuffer(TextBuff* s, char* pFillBuff, ssize_t buffSize, TEXT_BUFF_STYLE eStyle);
static k_Span2D TextBuffFrontBufferSpan(TextBuff* s);
static k_Span2D TextBuffBackBufferSpan(TextBuff* s);
static void TextBuffClearBackBuffer(TextBuff* s);
static void TextBuffPushDiff(TextBuff* s);
static void TextBuffResetBuffers(TextBuff* s);
static void TextBuffResizeBuffers(TextBuff* s, ssize_t width, ssize_t height);

typedef ssize_t (*PfnDecodeStr)(const void* pStrBuff, ssize_t strSize, ssize_t off, int32_t* pCode);

static ssize_t TextBuffStringFinal(
    TextBuff* s,
    int x,
    int y,
    TEXT_BUFF_STYLE eStyle,
    const void* pStrBuff,
    ssize_t strSize, /* Typed size, not bytesize. */
    PfnDecodeStr pfnDecodeStr,
    int maxSvLen
);

ssize_t TextBuffAdvanceWString(const void* pStrBuff, ssize_t strSize, ssize_t off, int32_t* pCode);
ssize_t TextBuffAdvanceUTF8(const void* pStrBuff, ssize_t strSize, ssize_t off, int32_t* pCode);

static ssize_t
TextBuffStringFinal(
    TextBuff* s,
    int x,
    int y,
    TEXT_BUFF_STYLE eStyle,
    const void* pStrBuff,
    ssize_t strSize, /* Typed size, not bytesize. */
    PfnDecodeStr pfnDecodeStr,
    int maxSvLen
)
{
    if (x < 0 || x >= s->tWidth || y < 0 || y >= s->tHeight)
        return 0;

    k_Span2D bb = TextBuffBackBufferSpan(s);

    int max = 0;
    wchar_t wc = 0;
    /* Tricky off by one loop. */
    for (ssize_t strOff = 0; strOff <= strSize; strOff += pfnDecodeStr(pStrBuff, strSize, strOff, &wc))
    {
        if (x >= s->tWidth || max >= maxSvLen) break;

        /* Zero width joiners and other non printable unicode stuff goes to hell. */
        int colWidth = 0;
        if ((uint32_t)wc == 0x200d || (colWidth = wcwidth(wc)) <= 0 || x + colWidth > s->tWidth)
            continue;

        TextBuffCell* pBBCell = k_Span2DGet(bb, x, y);
        pBBCell->wc = wc;
        pBBCell->eStyle = eStyle;

        for (int i = 1; i < colWidth; ++i)
        {
            if (x + i >= bb.width || y >= bb.height)
                return max;

            TextBuffCell* pBack = k_Span2DGet(bb, x + i, y);
            pBack->wc = -1;
            pBack->eStyle = eStyle;
        }

        x += colWidth;
        max += colWidth;
    }

    return max;
}

ssize_t
TextBuffAdvanceWString(const void* pStrBuff, ssize_t strSize, ssize_t off, int32_t* pCode)
{
    K_ASSERT(off <= strSize, "off: {sz}, strSize: {sz}", off, strSize);
    const wchar_t* p = pStrBuff;
    *pCode = p[off];
    return 1;
}

ssize_t
TextBuffAdvanceUTF8(const void* pStrBuff, ssize_t strSize, ssize_t off, int32_t* pCode)
{
    int err;
    *pCode = 0;
    return k_utf8_decode((const char*)pStrBuff + off, strSize, pCode, &err);
}

static bool
TextBuffCellEq(TextBuffCell* pLeft, TextBuffCell* pRight)
{
    return pLeft->eStyle == pRight->eStyle && pLeft->wc == pRight->wc;
}

#ifdef OPT_CHAFA
void showImages();
#endif

void
TextBuffPushChar(TextBuff* s, const char ch)
{
    VecCharPushVal(&s->vBuff, &s->pArena->base, ch);
}

void
TextBuffPush(TextBuff* s, const char* pBuff, const ssize_t buffSize)
{
    VecCharPushMany(&s->vBuff, &s->pArena->base, pBuff, buffSize);
}

void
TextBuffPushSv(TextBuff* s, const k_StringView svBuff)
{
    VecCharPushMany(&s->vBuff, &s->pArena->base, svBuff.pData, svBuff.size);
}

void
TextBuffFlush(TextBuff* s)
{
    VecChar* pvBuff = &s->vBuff;
    if (pvBuff->size > 0)
    {
        K_UNUSED const ssize_t n = write(STDOUT_FILENO, pvBuff->pData, pvBuff->size);
        pvBuff->size = 0;
    }
}

void
TextBuffMoveTopLeft(TextBuff* s)
{
    TextBuffPushSv(s, K_SV("\x1b[H"));
}

void
TextBuffUp(TextBuff* s, int steps)
{
    char aBuff[16] = {0};
    ssize_t n = k_print_toBufferSv(aBuff, K_ASIZE(aBuff), K_SV("\x1b[{i}A"), steps);
    TextBuffPush(s, aBuff, n);
}

void
TextBuffDown(TextBuff* s, int steps)
{
    char aBuff[16] = {0};
    ssize_t n = k_print_toBufferSv(aBuff, K_ASIZE(aBuff), K_SV("\x1b[{i}B"), steps);
    TextBuffPush(s, aBuff, n);
}

void
TextBuffForward(TextBuff* s, int steps)
{
    char aBuff[16] = {0};
    ssize_t n = k_print_toBufferSv(aBuff, K_ASIZE(aBuff), K_SV("\x1b[{i}C"), steps);
    TextBuffPush(s, aBuff, n);
}

void
TextBuffBack(TextBuff* s, int steps)
{
    char aBuff[16] = {0};
    ssize_t n = k_print_toBufferSv(aBuff, K_ASIZE(aBuff), K_SV("\x1b[{i}D"), steps);
    TextBuffPush(s, aBuff, n);
}

void
TextBuffMove(TextBuff* s, int x, int y)
{
    char aBuff[16] = {0};
    ssize_t n = k_print_toBufferSv(aBuff, K_ASIZE(aBuff), K_SV("\x1b[{i};{i}H"), y + 1, x + 1);
    TextBuffPush(s, aBuff, n);
}

void
TextBuffClearTermDown(TextBuff* s)
{
    TextBuffPushSv(s, K_SV("\x1b[0J"));
}

void
TextBuffClearTermUp(TextBuff* s)
{
    TextBuffPushSv(s, K_SV("\x1b[1J"));
}

void
TextBuffClearTerm(TextBuff* s)
{
    TextBuffPushSv(s, K_SV("\x1b[2J"));
}

void
TextBuffClearLine(TextBuff* s, TEXT_BUFF_ARG eArg)
{
    char aBuff[16] = {0};
    ssize_t n = k_print_toBufferSv(aBuff, K_ASIZE(aBuff), K_SV("\x1b[{i}K"), eArg);
    TextBuffPush(s, aBuff, n);
}

void
TextBuffHideCursor(TextBuff* s, bool bHide)
{
    bHide ? TextBuffPushSv(s, K_SV("\x1b[?25l")) : TextBuffPushSv(s, K_SV("\x1b[?25h"));
}

void
TextBuffPushWChar(TextBuff* s, wchar_t wc)
{
    char aBuff[8] = {0};
    int len = wctomb(aBuff, wc);

    if (len > 1) TextBuffPush(s, aBuff, len);
    else TextBuffPushChar(s, aBuff[0]);
}

void
TextBuffClearKittyImages(TextBuff* s)
{
    TextBuffPushSv(s, K_SV("\x1b_Ga=d,d=A\x1b\\"));
}

void
TextBuffSetTitle(TextBuff* s, const k_StringView svTitle)
{
    TextBuffPushSv(s, K_SV("\x1b]0;"));
    TextBuffPushSv(s, svTitle);
    TextBuffPushSv(s, K_SV("\a"));
}

void
TextBuffInit(TextBuff* s, k_Arena* pArena, ssize_t termWidth, ssize_t termHeight)
{
    *s = (TextBuff){0};

    s->pArena = pArena;

    K_ARENA_SCOPE_DELETERS(pArena)
    {
        TextBuffPushSv(s, K_SV(TEXT_BUFF_ALT_SCREEN_ENABLE));
        TextBuffPushSv(s, K_SV(TEXT_BUFF_KEYPAD_ENABLE));
        TextBuffPushSv(s, K_SV(TEXT_BUFF_MOUSE_ENABLE));
        TextBuffClearTerm(s);
        TextBuffMoveTopLeft(s);
        TextBuffHideCursor(s, true);
        TextBuffFlush(s);
        TextBuffResizeBuffers(s, termWidth, termHeight);
    }
}

void
TextBuffDestroy(TextBuff* s)
{
    K_ARENA_SCOPE_DELETERS(s->pArena)
    {
        // K_ASSERT(!s->oBuff.pvBuff->pData && s->oBuff.pvBuff->size == 0 && s->oBuff.pvBuff->cap == 0, "");

        TextBuffPresent(s);
        TextBuffHideCursor(s, false);
        TextBuffClearKittyImages(s);
        TextBuffMove(s, s->tWidth - 1, s->tHeight - 1);  /* Helps with default tty buffer. */
        TextBuffPushSv(s, K_SV(TEXT_BUFF_MOUSE_DISABLE));
        TextBuffPushSv(s, K_SV(TEXT_BUFF_KEYPAD_DISABLE));
        TextBuffPushSv(s, K_SV(TEXT_BUFF_ALT_SCREEN_DISABLE));
        TextBuffFlush(s);

        VecTextBuffCellDestroy(&s->vBack, &k_GpaInst()->base);
        VecTextBuffCellDestroy(&s->vFront, &k_GpaInst()->base);

#ifdef OPT_CHAFA
        m_imgArena.freeAll();
#endif
    }
}

void
TextBuffClean(TextBuff* s)
{
    if (s->bErase)
    {
        s->bErase = false;
        TextBuffClearTerm(s);
        TextBuffResetBuffers(s);
        TextBuffFlush(s);
    }

    if (s->bResize)
    {
        s->bResize = false;
        TextBuffResizeBuffers(s, s->newTWidth, s->newTHeight);
    }
    else
    {
        TextBuffClearBackBuffer(s);
    }
}

void
TextBuffPresent(TextBuff* s)
{
    if (s->bErase)
    {
        s->bErase = false;
        TextBuffClearTerm(s);
    }

    TextBuffPushDiff(s);

#ifdef OPT_CHAFA
    showImages();
#endif

    K_SWAP(s->vFront, s->vBack);

    TextBuffFlush(s);

    k_ArenaRunDeleters(s->pArena);
}

void
TextBuffErase(TextBuff* s)
{
    s->bErase = true;
}

void
TextBuffResize(TextBuff* s, ssize_t width, ssize_t height)
{
    s->bResize = true;
    s->newTWidth = width;
    s->newTHeight = height;
}

ssize_t
TextBuffString(TextBuff* s, int x, int y, TEXT_BUFF_STYLE eStyle, const k_StringView sv, int maxSvLen)
{
    return TextBuffStringFinal(s, x, y, eStyle, sv.pData, sv.size, TextBuffAdvanceUTF8, maxSvLen);
}

ssize_t
TextBuffWideString(TextBuff* s, int x, int y, TEXT_BUFF_STYLE eStyle, const wchar_t* pWStr, ssize_t wStrSize, int maxSvLen)
{
    return TextBuffStringFinal(s, x, y, eStyle, pWStr, wStrSize, TextBuffAdvanceWString, maxSvLen);
}

ssize_t
TextBuffStrings(TextBuff* s, int x, int y, TextBuffStyleSVPair* pStrings, ssize_t stringsSize, int maxSvLen)
{
    ssize_t xOff = 0;

    for (ssize_t i = 0; i < stringsSize; ++i)
    {
        xOff += TextBuffStringFinal(
            s,
            x + xOff,
            y,
            pStrings[i].eStyle,
            pStrings[i].sv.pData,
            pStrings[i].sv.size,
            TextBuffAdvanceUTF8,
            maxSvLen
        );
    }

    return xOff;
}

ssize_t
TextBuffWideStrings(TextBuff* s, int x, int y, TextBuffStylePWCharPair* pStrings, ssize_t stringsSize, int maxSvLen)
{
    ssize_t xOff = 0;

    for (ssize_t i = 0; i < stringsSize; ++i)
    {
        xOff += TextBuffStringFinal(
            s,
            x + xOff,
            y,
            pStrings[i].eStyle,
            pStrings[i].pWStr,
            pStrings[i].strSize,
            TextBuffAdvanceWString,
            maxSvLen
        );
    }

    return xOff;
}

static ssize_t
TextBuffStyleToBuffer(TextBuff* s, char* pFillBuff, ssize_t buffSize, TEXT_BUFF_STYLE eStyle)
{
    ssize_t n = 0;

    n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV("\x1b[0"));

    if (eStyle)
    {
        if (eStyle & TEXT_BUFF_STYLE_BOLD)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_BOLD);
        if (eStyle & TEXT_BUFF_STYLE_DIM)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_DIM);
        if (eStyle & TEXT_BUFF_STYLE_ITALIC)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_ITALIC);
        if (eStyle & TEXT_BUFF_STYLE_UNDERLINE)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_UNDERLINE);
        if (eStyle & TEXT_BUFF_STYLE_BLINK)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_BLINK);
        if (eStyle & TEXT_BUFF_STYLE_REVERSE)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_REVERSE);
        if (eStyle & TEXT_BUFF_STYLE_INVIS)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_INVIS);
        if (eStyle & TEXT_BUFF_STYLE_STRIKE)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_STRIKE);
        if (eStyle & TEXT_BUFF_STYLE_RED)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_RED);
        if (eStyle & TEXT_BUFF_STYLE_GREEN)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_GREEN);
        if (eStyle & TEXT_BUFF_STYLE_YELLOW)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_YELLOW);
        if (eStyle & TEXT_BUFF_STYLE_BLUE)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_BLUE);
        if (eStyle & TEXT_BUFF_STYLE_MAGENTA)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_MAGENTA);
        if (eStyle & TEXT_BUFF_STYLE_CYAN)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_CYAN);
        if (eStyle & TEXT_BUFF_STYLE_WHITE)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_WHITE);
        if (eStyle & TEXT_BUFF_STYLE_BG_RED)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_BG_RED);
        if (eStyle & TEXT_BUFF_STYLE_BG_GREEN)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_BG_GREEN);
        if (eStyle & TEXT_BUFF_STYLE_BG_YELLOW)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_BG_YELLOW);
        if (eStyle & TEXT_BUFF_STYLE_BG_BLUE)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_BG_BLUE);
        if (eStyle & TEXT_BUFF_STYLE_BG_MAGENTA)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_BG_MAGENTA);
        if (eStyle & TEXT_BUFF_STYLE_BG_CYAN)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_BG_CYAN);
        if (eStyle & TEXT_BUFF_STYLE_BG_WHITE)
            n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV(";{i}"), TEXT_BUFF_STYLE_CODE_BG_WHITE);
    }

    n += k_print_toBufferSv(pFillBuff + n, buffSize - n, K_SV("m"));

    return n;
}

static k_Span2D
TextBuffFrontBufferSpan(TextBuff* s)
{
    return (k_Span2D){
        .pData = s->vFront.pData,
        .width = s->tWidth,
        .height = s->tHeight,
        .stride = s->tWidth,
        .memberSize = sizeof(TextBuffCell),
    };
}

static k_Span2D
TextBuffBackBufferSpan(TextBuff* s)
{
    return (k_Span2D){
        .pData = s->vBack.pData,
        .width = s->tWidth,
        .height = s->tHeight,
        .stride = s->tWidth,
        .memberSize = sizeof(TextBuffCell),
    };
}

static void
TextBuffClearBackBuffer(TextBuff* s)
{
    for (ssize_t i = 0; i < s->vBack.size; ++i)
    {
        TextBuffCell* pCell = &s->vBack.pData[i];
        pCell->wc = L' ';
        pCell->eStyle = TEXT_BUFF_STYLE_NORM;
    }
}

static void
TextBuffPushDiff(TextBuff* s)
{
    TEXT_BUFF_STYLE eLastStyle = TEXT_BUFF_STYLE_NORM;
    const k_Span2D spFront = TextBuffFrontBufferSpan(s);
    const k_Span2D spBack = TextBuffBackBufferSpan(s);

    for (ssize_t rowI = 0; rowI < s->tHeight; ++rowI)
    {
        bool bMoved = false;
        ssize_t nForwards = 0;

        for (ssize_t colI = 0; colI < s->tWidth; ++colI)
        {
            TextBuffCell* pFront = k_Span2DGet(spFront, colI, rowI);
            TextBuffCell* pBack = k_Span2DGet(spBack, colI, rowI);

            if (pBack->wc != -1 && !TextBuffCellEq(pFront, pBack))
            {
                if (!bMoved)
                {
                    bMoved = true;
                    TextBuffMove(s, nForwards, rowI);
                    nForwards = 0;
                }
                else if (nForwards > 0)
                {
                    TextBuffForward(s, nForwards);
                    nForwards = 0;
                }

                if (pBack->eStyle != eLastStyle)
                {
                    char aBuff[32];
                    const ssize_t nBytes = TextBuffStyleToBuffer(s, aBuff, K_ASIZE(aBuff), pBack->eStyle);
                    TextBuffPush(s, aBuff, nBytes);
                    eLastStyle = pBack->eStyle;
                }

                if (pBack->wc == L'\0') TextBuffPushWChar(s, L' ');
                else TextBuffPushWChar(s, pBack->wc);
            }
            else
            {
                if (pBack->wc != -1)
                {
                    const ssize_t w = wcwidth(pBack->wc);
                    if (w > 0) nForwards += w;
                }
            }
        }
    }

    if (s->vBuff.size > 0) TextBuffPushSv(s, K_SV(TEXT_BUFF_NORM));
}

static void
TextBuffResetBuffers(TextBuff* s)
{
    for (ssize_t i = 0; i < s->vFront.size; ++i)
    {
        TextBuffCell* pCell = &s->vFront.pData[i];
        pCell->wc = L' ';
        pCell->eStyle = TEXT_BUFF_STYLE_NORM;
    }

    memcpy(s->vBack.pData, s->vFront.pData, s->vFront.size * sizeof(s->vFront.pData[0]));
}

static void
TextBuffResizeBuffers(TextBuff* s, ssize_t width, ssize_t height)
{
    if (width * height > 0)
    {
        VecTextBuffCellSetSize(&s->vBack, &k_GpaInst()->base, width * height);
        VecTextBuffCellSetSize(&s->vFront, &k_GpaInst()->base, width * height);
    }

    s->tWidth = width;
    s->tHeight = height;

    TextBuffResetBuffers(s);
}

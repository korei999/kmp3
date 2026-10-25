#pragma once

#include "klib/Arena.h"

#include "generated.h"

#include <wchar.h>

/* ansi code */
typedef enum
{
    TEXT_BUFF_STYLE_CODE_NORM = 0,
    TEXT_BUFF_STYLE_CODE_BOLD = 1,
    TEXT_BUFF_STYLE_CODE_DIM = 2,
    TEXT_BUFF_STYLE_CODE_ITALIC = 3,
    TEXT_BUFF_STYLE_CODE_UNDERLINE = 4,
    TEXT_BUFF_STYLE_CODE_BLINK = 5,
    TEXT_BUFF_STYLE_CODE_REVERSE = 7,
    TEXT_BUFF_STYLE_CODE_INVIS = 8,
    TEXT_BUFF_STYLE_CODE_STRIKE = 9,

    TEXT_BUFF_STYLE_CODE_RED = 31,
    TEXT_BUFF_STYLE_CODE_GREEN = 32,
    TEXT_BUFF_STYLE_CODE_YELLOW = 33,
    TEXT_BUFF_STYLE_CODE_BLUE = 34,
    TEXT_BUFF_STYLE_CODE_MAGENTA = 35,
    TEXT_BUFF_STYLE_CODE_CYAN = 36,
    TEXT_BUFF_STYLE_CODE_WHITE = 37,

    TEXT_BUFF_STYLE_CODE_BG_RED = 41,
    TEXT_BUFF_STYLE_CODE_BG_GREEN = 42,
    TEXT_BUFF_STYLE_CODE_BG_YELLOW = 43,
    TEXT_BUFF_STYLE_CODE_BG_BLUE = 44,
    TEXT_BUFF_STYLE_CODE_BG_MAGENTA = 45,
    TEXT_BUFF_STYLE_CODE_BG_CYAN = 46,
    TEXT_BUFF_STYLE_CODE_BG_WHITE = 47,
} TEXT_BUFF_STYLE_CODE;

typedef enum
{
    TEXT_BUFF_ARG_TO_END,
    TEXT_BUFF_ARG_TO_BEGINNING,
    TEXT_BUFF_ARG_EVERYTHING
} TEXT_BUFF_ARG;

typedef enum
{
    TEXT_BUFF_STYLE_NORM = 0,
    TEXT_BUFF_STYLE_BOLD = 1,
    TEXT_BUFF_STYLE_DIM = 1 << 1,
    TEXT_BUFF_STYLE_ITALIC = 1 << 2,
    TEXT_BUFF_STYLE_UNDERLINE = 1 << 3,
    TEXT_BUFF_STYLE_BLINK = 1 << 4,
    TEXT_BUFF_STYLE_REVERSE = 1 << 5,
    TEXT_BUFF_STYLE_INVIS = 1 << 6,
    TEXT_BUFF_STYLE_STRIKE = 1 << 7,

    TEXT_BUFF_STYLE_RED = 1 << 8,
    TEXT_BUFF_STYLE_GREEN = 1 << 9,
    TEXT_BUFF_STYLE_YELLOW = 1 << 10,
    TEXT_BUFF_STYLE_BLUE = 1 << 11,
    TEXT_BUFF_STYLE_MAGENTA = 1 << 12,
    TEXT_BUFF_STYLE_CYAN = 1 << 13,
    TEXT_BUFF_STYLE_WHITE = 1 << 14,

    TEXT_BUFF_STYLE_BG_RED = 1 << 15,
    TEXT_BUFF_STYLE_BG_GREEN = 1 << 16,
    TEXT_BUFF_STYLE_BG_YELLOW = 1 << 17,
    TEXT_BUFF_STYLE_BG_BLUE = 1 << 18,
    TEXT_BUFF_STYLE_BG_MAGENTA = 1 << 19,
    TEXT_BUFF_STYLE_BG_CYAN = 1 << 20,
    TEXT_BUFF_STYLE_BG_WHITE = 1 << 21,
} TEXT_BUFF_STYLE;

/* FIXME: this is not a good approach, there are characters that can be larger in size than sizeof(wchar_t) like '/̶̢̧̠̩̠̠̪̜͚͙̏͗̏̇̑̈͛͘ͅ' (41 bytes).
 * To represent 1 terminal cell as 1 character this has to be some kind of StringView structure. */
typedef struct
{
    wchar_t wc;
    TEXT_BUFF_STYLE eStyle;
} TextBuffCell;

typedef struct
{
    TEXT_BUFF_STYLE eStyle;
    k_StringView sv;
} TextBuffStyleSVPair;

typedef struct
{
    TEXT_BUFF_STYLE eStyle;
    wchar_t* pWStr;
    ssize_t strSize;
} TextBuffStylePWCharPair;

#define K_NAME VecTextBuffCell
#define K_TYPE TextBuffCell
#include "klib/VecGen-inc.h"

typedef struct
{
    k_Arena* pArena;

    VecChar vBuff;

    /* */

    ssize_t tWidth;
    ssize_t tHeight;

    ssize_t newTWidth;
    ssize_t newTHeight;

    bool bResize;
    bool bErase;

    VecTextBuffCell vFront; /* what to show */
    VecTextBuffCell vBack; /* where to write */
} TextBuff;

/* direct write api (cpu heavy) */
void TextBuffPushChar(TextBuff* s, const char ch);
void TextBuffPush(TextBuff* s, const char* pBuff, const ssize_t buffSize);
void TextBuffPushSv(TextBuff* s, const k_StringView svBuff);
void TextBuffFlush(TextBuff* s);
void TextBuffMoveTopLeft(TextBuff* s);
void TextBuffUp(TextBuff* s, int steps);
void TextBuffDown(TextBuff* s, int steps);
void TextBuffForward(TextBuff* s, int steps);
void TextBuffBack(TextBuff* s, int steps);
void TextBuffMove(TextBuff* s, int x, int y);
void TextBuffClearTermDown(TextBuff* s);
void TextBuffClearTermUp(TextBuff* s);
void TextBuffClearTerm(TextBuff* s);
void TextBuffClearLine(TextBuff* s, TEXT_BUFF_ARG eArg);
void TextBuffHideCursor(TextBuff* s, bool bHide);
void TextBuffPushWChar(TextBuff* s, wchar_t wc);
void TextBuffClearKittyImages(TextBuff* s);
void TextBuffSetTitle(TextBuff* s, const k_StringView svTitle);
/* */

/* main api (more efficient using damage tracking) */
void TextBuffInit(TextBuff* s, k_Arena* pArena, ssize_t termWidth, ssize_t termHeight);
void TextBuffDestroy(TextBuff* s);
void TextBuffClean(TextBuff* s);
void TextBuffPresent(TextBuff* s);
void TextBuffErase(TextBuff* s);
void TextBuffResize(TextBuff* s, ssize_t width, ssize_t height);

ssize_t TextBuffString(TextBuff* s, int x, int y, TEXT_BUFF_STYLE eStyle, const k_StringView sv, int maxSvLen);
ssize_t TextBuffWideString(TextBuff* s, int x, int y, TEXT_BUFF_STYLE eStyle, const wchar_t* pWStr, ssize_t wStrSize, int maxSvLen);

ssize_t TextBuffStrings(TextBuff* s, int x, int y, TextBuffStyleSVPair* pStrings, ssize_t stringsSize, int maxSvLen);
ssize_t TextBuffWideStrings(TextBuff* s, int x, int y, TextBuffStylePWCharPair* pStrings, ssize_t stringsSize, int maxSvLen);

#ifdef OPT_CHAFA
void image(int x, int y, const platform::chafa::Image& img);
void forceClean(int x, int y, int width, int height); /* remove images */
#endif
/* */

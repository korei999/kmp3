#include "window.hh"

#include "adt/logs.hh"
#include "app.hh"
#include "defaults.hh"
#include "common.hh"
#include "input.hh"

#include <cmath>

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define TB_IMPL
#define TB_OPT_ATTR_W 32
// #define TB_OPT_EGC
#include "termbox2/termbox2.h"

#ifdef __clang__
    #pragma clang diagnostic pop
#elif defined __GNUC__
    #pragma GCC diagnostic pop
#endif

namespace platform
{
namespace termbox2
{
namespace window
{

Arena* g_pFrameArena {};
bool g_bDrawHelpMenu = false;
u16 g_firstIdx = 0;

static bool s_bImage = false;

bool
init(Arena* pAlloc)
{
    [[maybe_unused]] int r = tb_init();
    assert(r == 0 && "'tb_init()' failed");

    tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);

    LOG_NOTIFY("tb_has_truecolor: {}\n", (bool)tb_has_truecolor());
    LOG_NOTIFY("tb_has_egc: {}\n", (bool)tb_has_egc());

    g_pFrameArena = pAlloc;

    input::fillInputMap();

    return r == 0;
}

void
destroy()
{
    tb_shutdown();
    LOG_NOTIFY("tb_shutdown()\n");
}

static common::READ_STATUS
readWChar(tb_event* pEv)
{
    namespace c = common;

    const auto& ev = *pEv;
    tb_peek_event(pEv, defaults::READ_TIMEOUT);

    if (ev.type == 0) return c::READ_STATUS::DONE;
    else if (ev.key == TB_KEY_ESC) return c::READ_STATUS::DONE;
    else if (ev.key == TB_KEY_CTRL_C) return c::READ_STATUS::DONE;
    else if (ev.key == TB_KEY_ENTER) return c::READ_STATUS::DONE;
    else if (ev.key == TB_KEY_CTRL_W)
    {
        if (c::g_input.idx > 0)
        {
            c::g_input.idx = 0;
            c::g_input.zeroOutBuff();
        }
    }
    else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2 || ev.key == TB_KEY_CTRL_H)
    {
        if (c::g_input.idx > 0)
            c::g_input.aBuff[--c::g_input.idx] = L'\0';
    }
    else if (ev.ch)
    {
        if (c::g_input.idx < utils::size(c::g_input.aBuff) - 1)
            c::g_input.aBuff[c::g_input.idx++] = ev.ch;
    }

    return c::READ_STATUS::OK_;
}

void
subStringSearch()
{
    tb_event ev {};

    common::subStringSearch(
        g_pFrameArena,
        +[](void* pArg) { return readWChar((tb_event*)pArg); },
        &ev,
        +[](void*) { draw(); },
        nullptr,
        &g_firstIdx
    );
}

void
seekFromInput()
{
    tb_event ev {};

    common::seekFromInput(
        +[](void* pArg) { return readWChar((tb_event*)pArg); },
        &ev,
        +[](void*) { draw(); },
        nullptr
    );
}

static void
procResize([[maybe_unused]] tb_event* pEv)
{
    app::g_pPlayer->bSelectionChanged = true;
}

void
procEvents()
{
    tb_event ev;
    tb_peek_event(&ev, defaults::UPDATE_RATE);
    auto height = tb_height();

    const auto& pl = *app::g_pPlayer;

    switch (ev.type)
    {
        default:
        assert(false && "what is this event?");
        break;

        case 0: break;

        case TB_EVENT_KEY:
        input::procKey(&ev);
        common::fixFirstIdx(height - 7 - pl.statusAndInfoHeight, &g_firstIdx);
        break;

        case TB_EVENT_RESIZE:
        procResize(&ev);
        break;

        case TB_EVENT_MOUSE:
        input::procMouse(&ev);
        common::fixFirstIdx(height - 7 - pl.statusAndInfoHeight, &g_firstIdx);
        break;
    }
}

static void
drawBox(
    const u16 x,
    const u16 y,
    const u16 width,
    const u16 height,
    const u32 fgColor = TB_WHITE,
    const u32 bgColor = TB_DEFAULT,
    const bool bCleanInside = false,
    const u32 tl = common::CHAR_TL,
    const u32 tr = common::CHAR_TR,
    const u32 bl = common::CHAR_BL,
    const u32 br = common::CHAR_BR,
    const u32 t = common::CHAR_T,
    const u32 b = common::CHAR_B,
    const u32 l = common::CHAR_L,
    const u32 r = common::CHAR_R
)
{
    const u16 termHeight = tb_height();
    const u16 termWidth = tb_width();

    if (bCleanInside)
    {
        for (int j = y + 1; j < y + height && j < termWidth; ++j)
            for (int i = x + 1; i < x + width; ++i)
                tb_set_cell(i, j, L' ', TB_WHITE, TB_DEFAULT);
    }

    if (x < termWidth && y < termHeight) /* top-left */
        tb_set_cell(x, y, tl, fgColor, bgColor);
    if (x + width < termWidth && y < termHeight) /* top-right */
        tb_set_cell(x + width, y, tr, fgColor, bgColor);
    if (x < termWidth && y + height < termHeight) /* bot-left */
        tb_set_cell(x, y + height, bl, fgColor, bgColor);
    if (x + width < termWidth && y + height < termHeight) /* bot-right */
        tb_set_cell(x + width, y + height, br, fgColor, bgColor);

    /* top */
    for (int i = x + 1, times = 0; times < width - 1; ++i, ++times)
        tb_set_cell(i, y, t, fgColor, bgColor);
    /* bot */
    for (int i = x + 1, times = 0; times < width - 1; ++i, ++times)
        tb_set_cell(i, y + height, b, fgColor, bgColor);
    /* left */
    for (int i = y + 1, times = 0; times < height - 1; ++i, ++times)
        tb_set_cell(x, i, l, fgColor, bgColor);
    /* right */
    for (int i = y + 1, times = 0; times < height - 1; ++i, ++times)
        tb_set_cell(x + width, i, r, fgColor, bgColor);
}

static void
drawWideString(
    const int x,
    const int y,
    const wchar_t* pBuff,
    const u32 buffSize,
    const long maxLen,
    const u32 fg = TB_WHITE,
    const u32 bg = TB_DEFAULT
)
{
    long i = 0;
    while (i < buffSize && pBuff[i] && i < maxLen - 2)
    {
        tb_set_cell(x + 1 + i, y, pBuff[i], fg, bg);
        ++i;
    }
}

static void
drawMBString(
    const int x,
    const int y,
    const String str,
    const long maxLen,
    const u32 fg = TB_WHITE,
    const u32 bg = TB_DEFAULT,
    const bool bWrap = false,
    const int xWrapAt = 0,
    const int nMaxWraps = 0
)
{
    long it = 0;
    long max = 0;
    int yOff = y;
    int xOff = x;
    int nWraps = 0;
    long maxLenMod = maxLen;

    // wchar_t* pWstr = (wchar_t*)zalloc(g_pFrameArena, str.size + 1, sizeof(wchar_t));
    // auto mbLen = mbstowcs(pWstr, str.pData, str.size);

    // for (long i = 0, wrapLen = 0; i < maxLen - 2 && i < (long)mbLen && pWstr[i]; ++i, ++wrapLen, ++xOff)
    // {
    //     tb_set_cell(xOff + 1, yOff, pWstr[i], fg, bg);
    // }

    wchar_t wc {};
    for (; it < str.size; ++max)
    {
        if (max >= maxLenMod - 2)
        {
            break;

            /* FIXME: breaks termbox */
            // if (bWrap)
            // {
            //     max = 0;
            //     maxLenMod = maxLen + (x - xWrapAt); /* string gets longer */
            //     xOff = xWrapAt;
            //     ++yOff;
            //     ++nWraps;
            //     if (nWraps > nMaxWraps) break;
            // }
            // else break;
        }

        int charLen = mbtowc(&wc, &str[it], str.size - it);
        if (charLen < 0) break;

        it += charLen;
        tb_set_cell(xOff + 1 + max, yOff, wc, fg, bg);
    }
}

static void
drawSongList()
{
    const auto& pl = *app::g_pPlayer;
    const auto off = pl.statusAndInfoHeight;
    const u16 listHeight = tb_height() - 5 - off;

    for (u16 h = g_firstIdx, i = 0; i < listHeight - 1 && h < pl.aSongIdxs.size; ++h, ++i)
    {
        const u16 songIdx = pl.aSongIdxs[h];
        const String sSong = pl.aShortArgvs[songIdx];
        const u32 maxLen = tb_width() - 2;

        bool bSelected = songIdx == pl.selected ? true : false;

        u32 fg = TB_WHITE;
        u32 bg = TB_DEFAULT;
        if (h == pl.focused && bSelected)
        {
            fg = TB_BLACK|TB_BOLD;
            bg = TB_YELLOW;
        }
        else if (h == pl.focused)
        {
            fg = TB_BLACK;
            bg = TB_WHITE;
        }
        else if (bSelected)
        {
            fg = TB_YELLOW|TB_BOLD;
        }

        drawMBString(1, i + off + 4, sSong, maxLen, fg, bg);
    }

    drawBox(0, off + 3, tb_width() - 1, listHeight, TB_BLUE, TB_DEFAULT);
}

using VOLUME_COLOR = int;

[[nodiscard]] static VOLUME_COLOR
drawVolume(Arena* pAlloc, const u16 split)
{
    const auto width = tb_width();
    const f32 vol = app::g_pMixer->volume;
    const bool bMuted = app::g_pMixer->bMuted;
    constexpr String fmt = "volume: %3d";
    const int maxVolumeBars = (split - fmt.size - 2) * vol * 1.0f/defaults::MAX_VOLUME;

    auto volumeColor = [&](int i) -> VOLUME_COLOR {
        f32 col = f32(i) / (f32(split - fmt.size - 2 - 1) * (1.0f/defaults::MAX_VOLUME));

        if (col <= 0.33f) return TB_GREEN;
        else if (col > 0.33f && col <= 0.66f) return TB_GREEN;
        else if (col > 0.66f && col <= 1.00f) return TB_YELLOW;
        else return TB_RED;
    };

    const auto col = bMuted ? TB_BLUE : volumeColor(maxVolumeBars - 1) | TB_BOLD;

    for (int i = fmt.size + 2, nTimes = 0; i < split && nTimes < maxVolumeBars; ++i, ++nTimes)
    {
        VOLUME_COLOR col;
        wchar_t wc;
        if (bMuted)
        {
            col = TB_BLUE;
            wc = common::CHAR_VOL;
        }
        else
        {
            col = volumeColor(nTimes);
            wc = common::CHAR_VOL_MUTED;
        }

        tb_set_cell(i, 2, wc, col, TB_DEFAULT);
    }

    char* pBuff = (char*)zalloc(pAlloc, 1, width + 1);
    snprintf(pBuff, width, fmt.pData, int(std::round(vol * 100.0f)));
    drawMBString(0, 2, pBuff, split + 1, col);

    return col;
}

static void
drawTime(const u16 split)
{
    const int width = tb_width();
    const String str = common::allocTimeString(g_pFrameArena, width);
    drawMBString(0, 4, str, split + 1);
}

static void
drawTotal(Arena* pAlloc, const u16 split)
{
    const auto width = tb_width();
    auto& pl = *app::g_pPlayer;

    char* pBuff = (char*)zalloc(pAlloc, 1, width + 1);

    int n = snprintf(pBuff, width, "total: %ld / %u", pl.selected, pl.aShortArgvs.size - 1);
    if (pl.eReapetMethod != PLAYER_REPEAT_METHOD::NONE)
    {
        const char* sArg {};
        if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::TRACK) sArg = "track";
        else if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST) sArg = "playlist";

        snprintf(pBuff + n, width - n, " (repeat %s)", sArg);
    }

    drawMBString(0, 1, pBuff, split + 1);
}

static void
drawStatus()
{
    const auto width = tb_width();
    const auto& pl = *app::g_pPlayer;
    const u16 split = std::round(f64(width) * pl.statusToInfoWidthRatio);

    drawTotal(g_pFrameArena, split);
    VOLUME_COLOR volumeColor = drawVolume(g_pFrameArena, split);
    drawTime(split);
    drawBox(0, 0, split, pl.statusAndInfoHeight + 1, volumeColor, TB_DEFAULT);
}

static void
drawInfo()
{
    const auto width = tb_width();
    const auto& pl = *app::g_pPlayer;
    const u16 split = std::round(f64(width) * pl.statusToInfoWidthRatio);
    const auto maxStringWidth = width - split - 1;

    drawBox(split + 1, 0, tb_width() - split - 2, pl.statusAndInfoHeight + 1, TB_BLUE, TB_DEFAULT);

    char* pBuff = (char*)zalloc(g_pFrameArena, 1, width + 1);

    /* title */
    {
        int n = print::toBuffer(pBuff, width, "title: ");
        drawMBString(split + 1, 1, pBuff, maxStringWidth);
        memset(pBuff, 0, width + 1);
        if (pl.info.title.size > 0)
            print::toBuffer(pBuff, width, "{}", pl.info.title);
        else print::toBuffer(pBuff, width, "{}", pl.aShortArgvs[pl.selected]);
        drawMBString(split + 1 + n, 1, pBuff, maxStringWidth - n - 1, TB_ITALIC|TB_BOLD|TB_YELLOW, TB_DEFAULT, true, split + 1, 1);
    }

    /* album */
    {
        memset(pBuff, 0, width + 1);
        int n = print::toBuffer(pBuff, width, "album: ");
        drawMBString(split + 1, 3, pBuff, maxStringWidth);
        if (pl.info.album.size > 0)
        {
            memset(pBuff, 0, width + 1);
            print::toBuffer(pBuff, width, "{}", pl.info.album);
            drawMBString(split + 1 + n, 3, pBuff, maxStringWidth - n - 1, TB_BOLD);
        }
    }

    /* artist */
    {
        memset(pBuff, 0, width + 1);
        int n = print::toBuffer(pBuff, width, "artist: ");
        drawMBString(split + 1, 4, pBuff, maxStringWidth);
        if (pl.info.artist.size > 0)
        {
            memset(pBuff, 0, width + 1);
            print::toBuffer(pBuff, width, "{}", pl.info.artist);
            drawMBString(split + 1 + n, 4, pBuff, maxStringWidth - n - 1, TB_BOLD);
        }
    }
}

static void
drawBottomLine()
{
    namespace c = common;

    const auto& pl = *app::g_pPlayer;
    char aBuff[16] {};
    auto height = tb_height();
    auto width = tb_width();

    print::toBuffer(aBuff, utils::size(aBuff) - 1, "{}", pl.focused);
    String str(aBuff);

    drawMBString(width - str.size - 2, height - 1, str, str.size + 2); /* yeah + 2 */

    if (c::g_input.eCurrMode != READ_MODE::NONE || (c::g_input.eCurrMode == READ_MODE::NONE && wcsnlen(c::g_input.aBuff, utils::size(c::g_input.aBuff)) > 0))
    {
        const String sSearching = c::readModeToString(c::g_input.eLastUsedMode);
        drawMBString(0, height - 1, sSearching, sSearching.size + 2);
        drawWideString(sSearching.size, height - 1, c::g_input.aBuff, utils::size(c::g_input.aBuff), width - sSearching.size);

        if (c::g_input.eCurrMode != READ_MODE::NONE)
        {
            u32 wlen = wcsnlen(c::g_input.aBuff, utils::size(c::g_input.aBuff));
            drawWideString(sSearching.size + wlen, height - 1, common::CURSOR_BLOCK, 1, 3);
        }
    }
}

static void
drawTimeSlider()
{
    const auto xOff = 2;
    const auto width = tb_width();
    const auto off = app::g_pPlayer->statusAndInfoHeight + 2;
    const auto& time = app::g_pMixer->currentTimeStamp;
    const auto& maxTime = app::g_pMixer->totalSamplesCount;
    const auto timePlace = (f64(time) / f64(maxTime)) * (width - 2 - xOff);

    if (app::g_pMixer->bPaused) tb_set_cell(1, off, '|', TB_WHITE|TB_BOLD, TB_DEFAULT);
    else tb_set_cell(1, off, '>', TB_WHITE|TB_BOLD, TB_DEFAULT);

    for (long i = xOff + 1; i < width - 1; ++i)
    {
        wchar_t wc = L'━';
        if ((i - 1) == std::floor(timePlace + xOff)) wc = L'╋';
        tb_set_cell(i, off, wc, TB_WHITE, TB_DEFAULT);
    }
}

void
draw()
{
    if (tb_height() < 6 || tb_width() < 6) return;

    tb_clear();

    if (tb_height() > 9 && tb_width() > 9)
    {

        if (app::g_pPlayer->bSelectionChanged)
        {
            app::g_pPlayer->bSelectionChanged = false;
        }

        drawTimeSlider();
        drawSongList();
        drawBottomLine();
    }

    drawStatus();
    drawInfo();

    tb_present();
}

} /* namespace window */
} /* namespace termbox2 */
} /* namespace platform */

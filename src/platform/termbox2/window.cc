#include "window.hh"

#include "adt/logs.hh"
#include "app.hh"
#include "defaults.hh"
#include "common.hh"
#include "input.hh"

#include <cmath>
#include <stdatomic.h>

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

#ifdef USE_CHAFA
    #include "platform/chafa/chafa.hh"
#endif

namespace platform
{
namespace termbox2
{
namespace window
{

Arena* g_pFrameArena {};
u16 g_firstIdx = 0;
int g_timeStringSize {};
f64 g_time {};

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

    switch (ev.type)
    {
        default:
        assert(false && "what is this event?");
        break;

        case 0: break;

        case TB_EVENT_KEY:
        input::procKey(&ev);
        common::fixFirstIdx(height - common::getHorizontalSplitPos(height) - 7, &g_firstIdx);
        break;

        case TB_EVENT_RESIZE:
        procResize(&ev);
        break;

        case TB_EVENT_MOUSE:
        input::procMouse(&ev);
        common::fixFirstIdx(height - common::getHorizontalSplitPos(height) - 7, &g_firstIdx);
        break;
    }
}

static void
clearArea(int x, int y, int width, int height)
{
    width = utils::min(width, tb_width());
    height = utils::min(height, tb_height());

    for (int j = y; j < height; ++j)
        for (int i = x; i < width; ++i)
            tb_set_cell(i, j, ' ', TB_DEFAULT, TB_DEFAULT);
}

static void
drawBox(
    u16 x,
    u16 y,
    u16 width,
    u16 height,
    u32 fgColor = TB_WHITE,
    u32 bgColor = TB_DEFAULT,
    bool bCleanInside = false,
    u32 tl = common::CHAR_TL,
    u32 tr = common::CHAR_TR,
    u32 bl = common::CHAR_BL,
    u32 br = common::CHAR_BR,
    u32 t = common::CHAR_T,
    u32 b = common::CHAR_B,
    u32 l = common::CHAR_L,
    u32 r = common::CHAR_R
)
{
    const u16 termHeight = tb_height();
    const u16 termWidth = tb_width();

    if (bCleanInside) clearArea(x + 1, y + 1, width - 1, height - 1);

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
    [[maybe_unused]] const bool bWrap = false,
    [[maybe_unused]] const int xWrapAt = 0,
    [[maybe_unused]] const int nMaxWraps = 0
)
{
    long it = 0;
    long max = 0;
    int yOff = y;
    int xOff = x;
    long maxLenMod = maxLen;
    // int nWraps = 0;

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
    const int height = tb_height();
    const int width = tb_width();
    const int split = common::getHorizontalSplitPos(height) + 3;
    const u16 listHeight = height - split - 2;

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

        drawMBString(1, i + split + 1, sSong, maxLen, fg, bg);
    }

    drawBox(0, split, width - 1, listHeight, TB_BLUE, TB_DEFAULT);
}

using VOLUME_COLOR = int;

static VOLUME_COLOR
drawVolume()
{
    const auto width = tb_width();
    const auto height = tb_height();
    int split = common::getHorizontalSplitPos(height) + 2;
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

        tb_set_cell(i, split, wc, col, TB_DEFAULT);
    }

    char* pBuff = (char*)zalloc(g_pFrameArena, 1, width + 1);
    snprintf(pBuff, width, fmt.pData, int(std::round(vol * 100.0f)));
    drawMBString(0, split, pBuff, split + 1, col);

    return col;
}

static void
drawInfo()
{
    const auto width = tb_width();
    const auto& pl = *app::g_pPlayer;

    /*drawBox(split + 1, 0, tb_width() - split - 2, pl.statusAndInfoHeight + 1, TB_BLUE, TB_DEFAULT);*/

    char* pBuff = (char*)zalloc(g_pFrameArena, 1, width + 1);

    /* title */
    {
        int n = print::toBuffer(pBuff, width, "title: ");
        drawMBString(0, 1, pBuff, width - 1);
        memset(pBuff, 0, width + 1);
        if (pl.info.title.size > 0)
            print::toBuffer(pBuff, width, "{}", pl.info.title);
        else print::toBuffer(pBuff, width, "{}", pl.aShortArgvs[pl.selected]);
        drawMBString(n, 1, pBuff, width - 1 - n, TB_ITALIC|TB_BOLD|TB_YELLOW, TB_DEFAULT);
    }

    /* album */
    {
        memset(pBuff, 0, width + 1);
        int n = print::toBuffer(pBuff, width, "album: ");
        drawMBString(0, 2, pBuff, width - 1);
        if (pl.info.album.size > 0)
        {
            memset(pBuff, 0, width + 1);
            print::toBuffer(pBuff, width, "{}", pl.info.album);
            drawMBString(n, 2, pBuff, width - 1 - n - 1, TB_BOLD);
        }
    }

    /* artist */
    {
        memset(pBuff, 0, width + 1);
        int n = print::toBuffer(pBuff, width, "artist: ");
        drawMBString(0, 3, pBuff, width - 2);
        if (pl.info.artist.size > 0)
        {
            memset(pBuff, 0, width + 1);
            print::toBuffer(pBuff, width, "{}", pl.info.artist);
            drawMBString(n, 3, pBuff, width - 1 - n - 1, TB_BOLD);
        }
    }
}

static void
drawBottomLine()
{
    namespace c = common;

    const auto& pl = *app::g_pPlayer;
    int height = tb_height();
    int width = tb_width();

    /* selected / focused */
    {
        char* pBuff = (char*)zalloc(g_pFrameArena, 1, width + 1);

        int n = print::toBuffer(pBuff, width, "{} / {}", pl.selected, pl.aShortArgvs.size - 1);
        if (pl.eReapetMethod != PLAYER_REPEAT_METHOD::NONE)
        {
            const char* sArg {};
            if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::TRACK) sArg = "track";
            else if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST) sArg = "playlist";

            n += print::toBuffer(pBuff + n, width - n, " (repeat {})", sArg);
        }

        // n += print::toBuffer(pBuff + n, width - 1, " | focused: {}", pl.focused);
        drawMBString(width - n - 2, height - 1, pBuff, width - 2);
    }

    if (
        c::g_input.eCurrMode != READ_MODE::NONE ||
        (c::g_input.eCurrMode == READ_MODE::NONE && wcsnlen(c::g_input.aBuff, utils::size(c::g_input.aBuff)) > 0)
    )
    {
        const String sReadMode = c::readModeToString(c::g_input.eLastUsedMode);
        drawWideString(sReadMode.size + 1, height - 1, c::g_input.aBuff, utils::size(c::g_input.aBuff), width - 2);
        drawMBString(1, height - 1, sReadMode, width - 2);

        if (c::g_input.eCurrMode != READ_MODE::NONE)
        {
            u32 wlen = wcsnlen(c::g_input.aBuff, utils::size(c::g_input.aBuff));
            drawWideString(sReadMode.size + wlen + 1, height - 1, common::CURSOR_BLOCK, 1, 3);
        }
    }
}

static void
drawTimeSlider()
{
    const auto& mix = *app::g_pMixer;

    int height = tb_height();
    int width = tb_width();

    int split = std::round(f64(height) * (1.0 - app::g_pPlayer->statusToInfoWidthRatio));
    int n = 0;

    /* time */
    {
        const String str = common::allocTimeString(g_pFrameArena, width);
        drawMBString(0, split + 1, str, width - 2);
        n += str.size;
    }

    /* play/pause indicator */
    {
        bool bPaused = atomic_load_explicit(&mix.bPaused, memory_order_relaxed);
        const char* ntsIndicator = bPaused ? "II" : "I>";

        drawMBString(1 + n, split + 1, ntsIndicator, width - 2, TB_BOLD);

        n += strlen(ntsIndicator);
    }
    g_timeStringSize = n;
    
    /* time slider */
    {
        const auto& time = mix.currentTimeStamp;
        const auto& maxTime = mix.totalSamplesCount;
        const auto timePlace = (f64(time) / f64(maxTime)) * (width - 2 - (n + 3));

        for (long i = n + 3, t = 0; i < width - 2; ++i, ++t)
        {
            wchar_t wch = L'━';
            if ((t - 0) == std::floor(timePlace)) wch = L'╋';

            tb_set_cell(i, split + 1, wch, TB_DEFAULT, TB_DEFAULT);
        }
    }
}

#ifdef USE_CHAFA
static void
drawCoverImage()
{
    int height = tb_height();
    int width = tb_width();
    int split = common::getHorizontalSplitPos(height);

    static f64 lastTime {};

    if (app::g_pPlayer->bSelectionChanged && g_time > lastTime + defaults::IMAGE_UPDATE_RATE_LIMIT)
    {
        lastTime = g_time;
        app::g_pPlayer->bSelectionChanged = false;

        auto oCover = audio::MixerGetCoverImage(app::g_pMixer);
        if (oCover)
        {
            /* FIXME: horrible screen flash */
            tb_invalidate();
            auto& img = oCover.data;

            f64 scaleFactor = f64(split - 1) / f64(img.height);
            int scaledWidth = std::round(img.width * scaleFactor / defaults::FONT_ASPECT_RATIO);
            int scaledHeight = std::round(img.height * scaleFactor);
            int hdiff = width - 2 - scaledWidth;
            int vdiff = split - 1 - scaledHeight;
            const int hOff = std::round(hdiff / 2.0);
            const int vOff = std::round(vdiff / 2.0);

            LOG_GOOD("hOff: {}, vOff: {}, split: {}\n", hOff, vOff, split);

            /*platform::chafa::showImage(g_pFrameArena, img, split - 1, width - 2, hOff + 2, 1);*/
            String sImg = platform::chafa::getImageString(g_pFrameArena, img, split - 1, width - 2);

            tb_set_cursor(hOff + 2, 0);
            tb_send(sImg.pData, sImg.size);
            tb_hide_cursor();
        }
    }
}
#else
    #define drawCoverImage(...) (void)0
#endif

void
draw()
{
    int height = tb_height();
    int width = tb_width();

    g_time = utils::timeNowMS();

    /*if (height < 6 || width < 6) return;*/

    tb_clear();

    if (height > 9 && width > 9)
    {
        drawCoverImage();

        drawTimeSlider();
        drawSongList();
        drawBottomLine();
        drawVolume();
        drawInfo();
    }

    tb_present();
}

} /* namespace window */
} /* namespace termbox2 */
} /* namespace platform */

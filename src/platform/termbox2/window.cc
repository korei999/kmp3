#include "window.hh"

#include "adt/logs.hh"
#include "adt/math.hh"

#include "app.hh"
#include "common.hh"
#include "defaults.hh"
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
#define TB_OPT_EGC
#include "termbox2/termbox2.h"

#ifdef __clang__
    #pragma clang diagnostic pop
#elif defined __GNUC__
    #pragma GCC diagnostic pop
#endif

#ifdef OPT_CHAFA
    #include "platform/chafa/chafa.hh"
#endif

namespace platform::termbox2::window
{

Arena* g_pFrameArena {};
i16 g_firstIdx = 0;
int g_prevImgWidth = 0;
int g_prevImgHeight = 0;

static f64 s_time {};
static f64 s_lastResizeTime {};
static bool s_bNoImage {};

bool
start(Arena* pAlloc)
{
    [[maybe_unused]] int r = tb_init();
    assert(r == 0 && "'tb_init()' failed");

    tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);

    LOG_NOTIFY("tb_has_truecolor: {}\n", (bool)tb_has_truecolor());
    LOG_NOTIFY("tb_has_egc: {}\n", (bool)tb_has_egc());

    g_pFrameArena = pAlloc;

    input::fillInputMap();
    adjustListHeight();

    return r == 0;
}

void
destroy()
{
    tb_shutdown();
    LOG_NOTIFY("tb_shutdown()\n");
}

static void
fixFirstIdx()
{
    common::fixFirstIdx(tb_height() - app::g_pPlayer->m_imgHeight - 5, &g_firstIdx);
}

static common::READ_STATUS
readWChar(tb_event* pEv)
{
    namespace c = common;

    const auto& ev = *pEv;
    tb_peek_event(pEv, defaults::READ_TIMEOUT);

    if (ev.type == 0)
    {
        return c::READ_STATUS::DONE;
    }
    else if (ev.key == TB_KEY_ESC || ev.key == TB_KEY_CTRL_C)
    {
        c::g_input.zeroOut();
        return c::READ_STATUS::DONE;
    }
    else if (ev.key == TB_KEY_ENTER)
    {
        return c::READ_STATUS::DONE;
    }
    else if (ev.key == TB_KEY_CTRL_W)
    {
        if (c::g_input.m_idx > 0)
        {
            c::g_input.m_idx = 0;
            c::g_input.zeroOut();
        }
    }
    else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2 || ev.key == TB_KEY_CTRL_H)
    {
        if (c::g_input.m_idx > 0)
            c::g_input.m_aBuff[--c::g_input.m_idx] = L'\0';
        else return c::READ_STATUS::BACKSPACE;
    }
    else if (ev.ch)
    {
        if (c::g_input.m_idx < utils::size(c::g_input.m_aBuff) - 1)
            c::g_input.m_aBuff[c::g_input.m_idx++] = ev.ch;
    }

    return c::READ_STATUS::OK_;
}

void
subStringSearch()
{
    common::subStringSearch<
        [](void*) { tb_event ev {}; return readWChar(&ev); },
        [](void*) { draw(); }
    >(g_pFrameArena, &g_firstIdx, {}, {});
}

long
getImgOffset()
{
    return (app::g_bNoImage || s_bNoImage) ? 2 : app::g_pPlayer->m_imgWidth + 2;
}

void
adjustListHeight()
{
    const int split = app::g_pPlayer->m_imgHeight + 1;
    const u16 listHeight = tb_height() - split - 2;
    app::g_pWin->m_listHeight = listHeight;
}

void
centerSelection()
{
}

void
seekFromInput()
{
    common::seekFromInput<
        [](void*) { tb_event ev {}; return readWChar(&ev); },
        [](void*) { draw(); }
    >(nullptr, nullptr);
}

static void
procResize([[maybe_unused]] tb_event* pEv)
{
    app::g_pPlayer->m_bSelectionChanged = true;
    s_lastResizeTime = utils::timeNowMS();
    adjustListHeight();
}

void
procEvents()
{
    tb_event ev {};
    tb_peek_event(&ev, defaults::UPDATE_RATE);

    switch (ev.type)
    {
        default:
        break;

        case 0: break;

        case TB_EVENT_KEY:
        input::procKey(&ev);
        break;

        case TB_EVENT_RESIZE:
        procResize(&ev);
        break;

        case TB_EVENT_MOUSE:
        input::procMouse(&ev);
        break;
    }

    fixFirstIdx();
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
clearAreaHARD(int x, int y, int width, int height)
{
    width = utils::min(width, tb_width());
    height = utils::min(height, tb_height());

    char* pSpace = (char*)g_pFrameArena->zalloc(1, width + 1);
    utils::fill(pSpace, ' ', width);

    const char s_ntsNORM[] = "\x1b[0m";

    for (int h = y; h < height; ++h)
    {
        tb_set_cursor(x, h);
        tb_send(s_ntsNORM, sizeof(s_ntsNORM)); /* stupid fix for white lines */
        tb_send(pSpace, width);
    }
    tb_hide_cursor();
}

[[maybe_unused]] static void
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
    [[maybe_unused]] const long maxLen,
    const u32 fg = TB_WHITE,
    const u32 bg = TB_DEFAULT,
    [[maybe_unused]] const bool bWrap = false,
    [[maybe_unused]] const int xWrapAt = 0,
    [[maybe_unused]] const int nMaxWraps = 0
)
{
    tb_printf(x, y, fg, bg, "%.*s", str.getSize(), str.data());
}

static void
drawSongList()
{
    const auto& pl = *app::g_pPlayer;
    const auto& win = *app::g_pWin;
    const int split = pl.m_imgHeight + 1;

    const auto& aIdxBuff = pl.m_vSearchIdxs;

    for (ssize h = g_firstIdx, i = 0; i < win.m_listHeight - 1 && h < aIdxBuff.getSize(); ++h, ++i)
    {
        const u16 songIdx = aIdxBuff[h];
        const String sSong = pl.m_vShortSongs[songIdx];
        const u32 maxLen = tb_width() - 2;

        const bool bSelected = songIdx == pl.m_selected ? true : false;

        u32 fg = TB_WHITE;
        u32 bg = TB_DEFAULT;
        if (h == pl.m_focused && bSelected)
        {
            fg = TB_BLACK | TB_BOLD;
            bg = TB_YELLOW;
        }
        else if (h == pl.m_focused)
        {
            fg = TB_BLACK;
            bg = TB_WHITE;
        }
        else if (bSelected)
        {
            fg = TB_YELLOW | TB_BOLD;
        }

        drawMBString(1, i + split + 1, sSong, maxLen, fg, bg);
    }
}

using VOLUME_COLOR = int;

static VOLUME_COLOR
drawVolume()
{
    const auto width = tb_width();
    const long xOff = window::getImgOffset();

    const f32 vol = app::g_pMixer->getVolume();
    const bool bMuted = app::g_pMixer->isMuted();

    char* pBuff = (char*)g_pFrameArena->zalloc(1, width + 1);
    u32 n = print::toBuffer(pBuff, width, "volume: {:>3}", int(std::round(app::g_pMixer->getVolume() * 100.0)));

    const int maxVolumeBars = (width - xOff - n - 2) * vol * 1.0f/defaults::MAX_VOLUME;

    auto volumeColor = [&](int i) -> VOLUME_COLOR {
        f32 col = f32(i) / (f32(width - xOff - n - 2 - 1) * (1.0f/defaults::MAX_VOLUME));

        if (col <= 0.33f) return TB_GREEN;
        else if (col > 0.33f && col <= 0.66f) return TB_GREEN;
        else if (col > 0.66f && col <= 1.00f) return TB_YELLOW;
        else return TB_RED;
    };

    const auto col = bMuted ? TB_BLUE : volumeColor(maxVolumeBars - 1) | TB_BOLD;

    drawMBString(xOff, 6, pBuff, n, col, TB_DEFAULT);

    for (int i = xOff + n + 1, nTimes = 0; i < width && nTimes < maxVolumeBars; ++i, ++nTimes)
    {
        u32 barCol {};
        wchar_t wc;
        if (bMuted)
        {
            barCol |= TB_BLUE;
            wc = common::CHAR_VOL;
        }
        else
        {
            barCol |= volumeColor(nTimes);
            wc = common::CHAR_VOL_MUTED;
        }

        tb_set_cell(i, 6, wc, barCol, TB_DEFAULT);
    }

    return col;
}

static void
drawInfo()
{
    const auto width = tb_width();
    const auto& pl = *app::g_pPlayer;
    const long xOff = window::getImgOffset();

    char* pBuff = (char*)g_pFrameArena->zalloc(1, width + 1);

    /* title */
    {
        int n = print::toBuffer(pBuff, width, "title: ");
        drawMBString(xOff, 1, pBuff, width - 1);
        memset(pBuff, 0, width + 1);
        if (pl.m_info.title.getSize() > 0)
            print::toBuffer(pBuff, width, "{}", pl.m_info.title);
        else print::toBuffer(pBuff, width, "{}", pl.m_vShortSongs[pl.m_selected]);
        drawMBString(xOff + n + 1, 1, pBuff, width - 1 - n - 1, TB_ITALIC|TB_BOLD|TB_YELLOW, TB_DEFAULT);
    }

    /* album */
    {
        memset(pBuff, 0, width + 1);
        int n = print::toBuffer(pBuff, width, "album: ");
        drawMBString(xOff, 2, pBuff, width - 1);
        if (pl.m_info.album.getSize() > 0)
        {
            memset(pBuff, 0, width + 1);
            print::toBuffer(pBuff, width, "{}", pl.m_info.album);
            drawMBString(xOff + n + 1, 2, pBuff, width - 1 - n - 1, TB_BOLD);
        }
    }

    /* artist */
    {
        memset(pBuff, 0, width + 1);
        int n = print::toBuffer(pBuff, width, "artist: ");
        drawMBString(xOff, 3, pBuff, width - 2);
        if (pl.m_info.artist.getSize() > 0)
        {
            memset(pBuff, 0, width + 1);
            print::toBuffer(pBuff, width, "{}", pl.m_info.artist);
            drawMBString(xOff + n + 1, 3, pBuff, width - 1 - n - 1, TB_BOLD);
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
        char* pBuff = (char*)g_pFrameArena->zalloc(1, width + 1);

        int n = print::toBuffer(pBuff, width, "{} / {}", pl.m_selected, pl.m_vShortSongs.getSize() - 1);
        if (pl.m_eReapetMethod != PLAYER_REPEAT_METHOD::NONE)
        {
            const char* sArg {};
            if (pl.m_eReapetMethod == PLAYER_REPEAT_METHOD::TRACK) sArg = "track";
            else if (pl.m_eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST) sArg = "playlist";

            n += print::toBuffer(pBuff + n, width - n, " (repeat {})", sArg);
        }

        drawMBString(width - n - 2, height - 1, pBuff, width - 2);
    }

    if (
        c::g_input.m_eCurrMode != WINDOW_READ_MODE::NONE ||
        (c::g_input.m_eCurrMode == WINDOW_READ_MODE::NONE && wcsnlen(c::g_input.m_aBuff, utils::size(c::g_input.m_aBuff)) > 0)
    )
    {
        const String sReadMode = c::readModeToString(c::g_input.m_eLastUsedMode);
        drawWideString(sReadMode.getSize() + 1, height - 1, c::g_input.m_aBuff, utils::size(c::g_input.m_aBuff), width - 2);
        drawMBString(1, height - 1, sReadMode, width - 2);

        if (c::g_input.m_eCurrMode != WINDOW_READ_MODE::NONE)
        {
            u32 wlen = wcsnlen(c::g_input.m_aBuff, utils::size(c::g_input.m_aBuff));
            drawWideString(sReadMode.getSize() + wlen + 1, height - 1, common::CURSOR_BLOCK, 1, 3);
        }
    }
}

static void
drawTimeSlider()
{
    const auto& mix = *app::g_pMixer;
    int width = tb_width();
    const long xOff = window::getImgOffset();

    /* time */
    {
        const String str = common::allocTimeString(g_pFrameArena, width);
        drawMBString(xOff, 9, str, width - 2);
        str.getSize();
    }

    /* play/pause indicator */
    {
        bool bPaused = mix.isPaused().load(std::memory_order_relaxed);
        const char* ntsIndicator = bPaused ? "I>" : "II";

        drawMBString(xOff, 10, ntsIndicator, width - 2, TB_BOLD);
    }
    
    /* time slider */
    {
        const auto& time = mix.getCurrentTimeStamp();
        const auto& maxTime = mix.getTotalSamplesCount();
        const auto timePlace = (f64(time) / f64(maxTime)) * (width - xOff - 5);

        for (long i = xOff + 4, t = 0; i < width - 1; ++i, ++t)
        {
            wchar_t wch = L'━';
            if ((t - 0) == std::floor(timePlace)) wch = L'╋';

            tb_set_cell(i, 10, wch, TB_DEFAULT, TB_DEFAULT);
        }
    }
}

#ifdef OPT_CHAFA
static void
drawCoverImage()
{
    auto& pl = app::player();

    if (pl.m_bSelectionChanged && s_time > s_lastResizeTime + defaults::IMAGE_UPDATE_RATE_LIMIT)
    {
        pl.m_bSelectionChanged = false;

        auto oCover = app::g_pMixer->getCoverImage();

        /* termbox2 doesn't support image printing, these are bunch of ugly heuristics */
        if (s_bNoImage && oCover)
        {
            tb_invalidate();
        }
        else if (oCover || (!s_bNoImage && !oCover))
        {
            /* clear without flickering */
            const char sKittyClear[] = "\x1b_Ga=d,d=A\x1b\\";
            tb_send(sKittyClear, sizeof(sKittyClear));
            /* BUG: termbox2 still leaves some visible damage */
            clearAreaHARD(1, 1, g_prevImgWidth + 1, g_prevImgHeight + 1);
        }

        if (oCover)
        {
            namespace ch = platform::chafa;
            auto& img = oCover.value();
            ch::IMAGE_LAYOUT eLayout = app::g_bSixelOrKitty ? ch::IMAGE_LAYOUT::RAW : ch::IMAGE_LAYOUT::LINES;

            g_prevImgWidth = pl.m_imgWidth;
            g_prevImgHeight = pl.m_imgHeight;

            const auto chafaImg = ch::allocImage(
                g_pFrameArena, eLayout, img, pl.m_imgHeight, pl.m_imgWidth
            );

            f64 asp = (((f64)img.width / (f64)img.height));
            f64 normalAsp = 1920.0 / 1080.0;
            f64 diff = normalAsp - asp;
            int xOff = 0;

            /* try to center the image */
            if (!math::eq(diff, 0.0))
            {
                f64 coef = asp / normalAsp;
                f64 move = (1.0 - coef) / 2.0;

                xOff = std::floor(pl.m_imgWidth * move);
            }

            if (eLayout == ch::IMAGE_LAYOUT::RAW)
            {
                /* BUG: kitty shifts the image further somehow */
                if (app::g_eTerm == app::TERM::KITTY)
                    tb_set_cursor(1 + xOff/2, 1);
                else
                    tb_set_cursor(1 + xOff, 1);

                tb_send(chafaImg.uData.sRaw.data(), chafaImg.uData.sRaw.getSize());
            }
            else
            {
                for (ssize lineIdx = 1, i = 0; lineIdx < chafaImg.uData.vLines.getSize(); ++lineIdx, ++i)
                {
                    tb_set_cursor(1 + xOff, lineIdx);
                    const auto& sLine = chafaImg.uData.vLines[i];
                    tb_send(sLine.data(), sLine.getSize());
                }
            }

            tb_hide_cursor();
            s_bNoImage = false;
        }
        else
        {
            s_bNoImage = true;
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

    s_time = utils::timeNowMS();

    tb_clear();

    if (height > 9 && width > 9)
    {
        if (!app::g_bNoImage)
            drawCoverImage();

        drawTimeSlider();
        drawSongList();
        drawBottomLine();
        drawVolume();
        drawInfo();
    }

    tb_present();
}

} /* platform::termbox2::window */

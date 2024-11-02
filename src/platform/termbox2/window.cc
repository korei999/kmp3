#include "window.hh"

#include "input.hh"
#include "adt/Arr.hh"
#include "adt/defer.hh"
#include "app.hh"
#include "defaults.hh"
#include "logs.hh"

#define TB_IMPL
#define TB_OPT_ATTR_W 32
// #define TB_OPT_EGC
#include "termbox2/termbox2.h"

namespace platform
{
namespace termbox2
{
namespace window
{

enum READ_MODE : u8 {NONE, SEARCH, SEEK};

bool g_bDrawHelpMenu = false;
u16 g_firstIdx = 0;

static struct {
    wchar_t aBuff[64] {};
    u32 idx = 0;
    READ_MODE eCurrMode {};
    READ_MODE eLastUsedMode {};
} s_input {};

constexpr String
ReadModeToString(READ_MODE e)
{
    constexpr String map[] {"", "searching: ", "time: "};
    return map[e];
}

/* fix song list range after focus change */
static void
fixFirstIdx()
{
    const auto& pl = *app::g_pPlayer;
    const auto off = pl.statusAndInfoHeight;
    const u16 listHeight = tb_height() - 7 - off;

    const u16 focused = pl.focused;
    u16 first = g_firstIdx;

    if (focused > first + listHeight)
        first = focused - listHeight;
    else if (focused < first)
        first = focused;
    else if (pl.aSongIdxs.size < listHeight)
        first = 0;

    g_firstIdx = first;
}

void
init()
{
    [[maybe_unused]] int r = tb_init();
    assert(r == 0 && "'tb_init()' failed");

    tb_set_input_mode(TB_INPUT_ESC|TB_INPUT_MOUSE);

    LOG_NOTIFY("tb_has_truecolor: {}\n", (bool)tb_has_truecolor());
    LOG_NOTIFY("tb_has_egc: {}\n", (bool)tb_has_egc());
}

void
stop()
{
    tb_shutdown();
    LOG_NOTIFY("tb_shutdown()\n");
}

enum READ_STATUS : u8 { OK, DONE };

static READ_STATUS
readWChar(tb_event* pEv)
{
    const auto& ev = *pEv;
    tb_peek_event(pEv, defaults::READ_TIMEOUT);

    if (ev.type == 0) return READ_STATUS::DONE;
    else if (ev.key == TB_KEY_ESC) return READ_STATUS::DONE;
    else if (ev.key == TB_KEY_CTRL_C) return READ_STATUS::DONE;
    else if (ev.key == TB_KEY_ENTER) return READ_STATUS::DONE;
    else if (ev.key == TB_KEY_CTRL_W)
    {
        if (s_input.idx > 0)
        {
            s_input.idx = 0;
            utils::fill(s_input.aBuff, L'\0', utils::size(s_input.aBuff));
        }
    }
    else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2 || ev.key == TB_KEY_CTRL_H)
    {
        if (s_input.idx > 0)
            s_input.aBuff[--s_input.idx] = L'\0';
    }
    else if (ev.ch)
    {
        if (s_input.idx < utils::size(s_input.aBuff) - 1)
            s_input.aBuff[s_input.idx++] = ev.ch;
    }

    return READ_STATUS::OK;
}

void
subStringSearch(Allocator* pAlloc)
{
    tb_event ev {};
    auto& pl = *app::g_pPlayer;

    s_input.eLastUsedMode = s_input.eCurrMode = READ_MODE::SEARCH;
    defer( s_input.eCurrMode = READ_MODE::NONE );

    utils::fill(s_input.aBuff, L'\0', utils::size(s_input.aBuff));
    s_input.idx = 0;

    do
    {
        PlayerSubStringSearch(&pl, pAlloc, s_input.aBuff, utils::size(s_input.aBuff));
        g_firstIdx = 0;
        render(pAlloc);
    } while (readWChar(&ev) != READ_STATUS::DONE);

    /* fix focused if it ends up out of the list range */
    if (pl.focused >= (VecSize(&pl.aSongIdxs)))
        pl.focused = g_firstIdx;
}

static void
parseSeekThenRun()
{
    bool bPercent = false;
    bool bColon = false;

    Arr<char, 32> aMinutesBuff {};
    Arr<char, 32> aSecondsBuff {};

    const auto& buff = s_input.aBuff;
    for (long i = 0; buff[i] && i < long(utils::size(buff)); ++i)
    {
        if (buff[i] == L'%') bPercent = true;
        else if (buff[i] == L':')
        {
            /* leave if there is one more colon or bPercent */
            if (bColon || bPercent) break;
            bColon = true;
        }
        else if (iswdigit(buff[i]))
        {
            Arr<char, 32>* pTargetArray = bColon ? &aSecondsBuff : &aMinutesBuff;
            if (i < ArrCap(pTargetArray) - 1)
                ArrPush(pTargetArray, char(buff[i]));
        }
    }

    if (aMinutesBuff.size == 0) return;

    if (bPercent)
    {
        long maxMS = audio::MixerGetMaxMS(app::g_pMixer);

        audio::MixerSeekMS(app::g_pMixer, maxMS * (f64(atoll(aMinutesBuff.pData)) / 100.0));
    }
    else
    {
        long sec;
        if (aSecondsBuff.size == 0) sec = atoll(aMinutesBuff.pData);
        else sec = atoll(aSecondsBuff.pData) + atoll(aMinutesBuff.pData)*60;

        audio::MixerSeekMS(app::g_pMixer, sec * 1000);
    }
}

void
seekFromInput(Allocator* pAlloc)
{
    tb_event ev {};

    s_input.eLastUsedMode = s_input.eCurrMode = READ_MODE::SEEK;
    defer( s_input.eCurrMode = READ_MODE::NONE );

    utils::fill(s_input.aBuff, L'\0', utils::size(s_input.aBuff));
    s_input.idx = 0;

    do
    {
        render(pAlloc);
    } while (readWChar(&ev) != READ_STATUS::DONE);

    parseSeekThenRun();
}

static void
procResize([[maybe_unused]] tb_event* pEv)
{
    //
}

void
procEvents(Allocator* pAlloc)
{
    tb_event ev;
    tb_peek_event(&ev, defaults::UPDATE_RATE);

    switch (ev.type)
    {
        default:
        assert(false && "what is this event?");
        break;

        case 0: break;

        case TB_EVENT_KEY:
        input::procKey(&ev, pAlloc);
        fixFirstIdx();
        break;

        case TB_EVENT_RESIZE:
        procResize(&ev);
        break;

        case TB_EVENT_MOUSE:
        input::procMouse(&ev);
        fixFirstIdx();
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
    const u32 tl = L'┏',
    const u32 tr = L'┓',
    const u32 bl = L'┗',
    const u32 br = L'┛',
    const u32 t = L'━',
    const u32 b = L'━',
    const u32 l = L'┃',
    const u32 r = L'┃'
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
drawUtf8String(
    const int x,
    const int y,
    const String str,
    const long maxLen,
    const u32 fg = TB_WHITE,
    const u32 bg = TB_DEFAULT
    // const bool bWrap = false,
    // const int xWrapOrigin = 0,
    // const int nMaxWraps = 0
)
{
    long it = 0;
    long max = 0;
    int yOff = y;
    int xOff = x;
    // int nWraps = 0;

    for (; it < str.size; ++max)
    {
        if (max >= maxLen - 2)
        {
            /* FIXME: breaks the whole termbox */
            // if (bWrap)
            // {
            //     max = 0;
            //     xOff = xWrapOrigin;
            //     ++yOff;
            //     if (++nWraps > nMaxWraps) break;
            // }
            // else break;
            break;
        }

        wchar_t wc {};
        int charLen = mbtowc(&wc, &str[it], str.size - it);
        if (charLen < 0) return;

        it += charLen;
        tb_set_cell(xOff + 1 + max, yOff, wc, fg, bg);
    }
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

        drawUtf8String(1, i + off + 4, sSong, maxLen, fg, bg);
    }

    drawBox(0, off + 3, tb_width() - 1, listHeight, TB_BLUE, TB_DEFAULT);
}

using VOLUME_COLOR = int;

[[nodiscard]] static VOLUME_COLOR
drawVolume(Allocator* pAlloc, const u16 split)
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
            wc = L'▯';
        }
        else
        {
            col = volumeColor(nTimes);
            wc = L'▮';
        }

        tb_set_cell(i, 2, wc, col, TB_DEFAULT);
    }

    char* pBuff = (char*)alloc(pAlloc, 1, width);
    utils::fill(pBuff, '\0', width);
    snprintf(pBuff, width, fmt.pData, int(std::round(vol * 100.0f)));
    drawUtf8String(0, 2, pBuff, split + 1, col);

    return col;
}

static void
drawTime(Allocator* pAlloc, const u16 split)
{
    const auto width = tb_width();
    auto& mixer = *app::g_pMixer;

    char* pBuff = (char*)alloc(pAlloc, 1, width);
    utils::fill(pBuff, '\0', width);

    u64 t = std::round(f64(mixer.currentTimeStamp) / f64(mixer.nChannels) / f64(mixer.changedSampleRate));
    u64 totalT = std::round(f64(mixer.totalSamplesCount) / f64(mixer.nChannels) / f64(mixer.changedSampleRate));

    u64 currMin = t / 60;
    u64 currSec = t - (60 * currMin);

    u64 maxMin = totalT / 60;
    u64 maxSec = totalT - (60 * maxMin);

    int n = snprintf(pBuff, width, "time: %lu:%02lu / %lu:%02lu", currMin, currSec, maxMin, maxSec);
    if (mixer.sampleRate != mixer.changedSampleRate)
        snprintf(pBuff + n, width - n, " (%d%% speed)", int(std::round(f64(mixer.changedSampleRate) / f64(mixer.sampleRate) * 100.0)));

    drawUtf8String(0, 4, pBuff, split + 1);
}

static void
drawTotal(Allocator* pAlloc, const u16 split)
{
    const auto width = tb_width();
    auto& pl = *app::g_pPlayer;

    char* pBuff = (char*)alloc(pAlloc, 1, width);
    utils::fill(pBuff, '\0', width);

    int n = snprintf(pBuff, width, "total: %ld / %u", pl.selected, pl.aShortArgvs.size - 1);
    if (pl.eReapetMethod != PLAYER_REPEAT_METHOD::NONE)
    {
        const char* sArg {};
        if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::TRACK) sArg = "track";
        else if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST) sArg = "playlist";

        snprintf(pBuff + n, width - n, " (repeat %s)", sArg);
    }

    drawUtf8String(0, 1, pBuff, split + 1);
}

static void
drawStatus(Allocator* pAlloc)
{
    const auto width = tb_width();
    const auto& pl = *app::g_pPlayer;
    const u16 split = std::round(f64(width) * pl.statusToInfoWidthRatio);

    drawTotal(pAlloc, split);
    VOLUME_COLOR volumeColor = drawVolume(pAlloc, split);
    drawTime(pAlloc, split);
    drawBox(0, 0, split, pl.statusAndInfoHeight + 1, volumeColor, TB_DEFAULT);
}

static void
drawInfo(Allocator* pAlloc)
{
    const auto width = tb_width();
    const auto& pl = *app::g_pPlayer;
    const u16 split = std::round(f64(width) * pl.statusToInfoWidthRatio);
    const auto maxStringWidth = width - split - 1;

    drawBox(split + 1, 0, tb_width() - split - 2, pl.statusAndInfoHeight + 1, TB_BLUE, TB_DEFAULT);

    char* pBuff = (char*)alloc(pAlloc, 1, width);
    utils::fill(pBuff, '\0', width);

    /* title */
    {
        int n = print::toBuffer(pBuff, width, "title: ");
        drawUtf8String(split + 1, 1, pBuff, maxStringWidth);
        utils::fill(pBuff, '\0', width);
        if (pl.info.title.size > 0)
            print::toBuffer(pBuff, width, "{}", pl.info.title);
        else print::toBuffer(pBuff, width, "{}", pl.aShortArgvs[pl.selected]);
        drawUtf8String(split + 1 + n, 1, pBuff, maxStringWidth - n, TB_ITALIC|TB_BOLD|TB_YELLOW, TB_DEFAULT);
    }

    /* album */
    {
        utils::fill(pBuff, '\0', width);
        int n = print::toBuffer(pBuff, width, "album: ");
        drawUtf8String(split + 1, 3, pBuff, maxStringWidth);
        if (pl.info.album.size > 0)
        {
            utils::fill(pBuff, '\0', width);
            print::toBuffer(pBuff, width, "{}", pl.info.album);
            drawUtf8String(split + 1 + n, 3, pBuff, maxStringWidth - n, TB_BOLD);
        }
    }

    /* artist */
    {
        utils::fill(pBuff, '\0', width);
        int n = print::toBuffer(pBuff, width, "artist: ");
        drawUtf8String(split + 1, 4, pBuff, maxStringWidth);
        if (pl.info.artist.size > 0)
        {
            utils::fill(pBuff, '\0', width);
            print::toBuffer(pBuff, width, "{}", pl.info.artist);
            drawUtf8String(split + 1 + n, 4, pBuff, maxStringWidth - n, TB_BOLD);
        }
    }
}

static void
drawBottomLine()
{
    const auto& pl = *app::g_pPlayer;
    char aBuff[16] {};
    auto height = tb_height();
    auto width = tb_width();

    print::toBuffer(aBuff, utils::size(aBuff), "{}", pl.focused);
    String str(aBuff);

    drawUtf8String(width - str.size - 2, height - 1, str, str.size + 2); /* yeah + 2 */

    if (s_input.eCurrMode != READ_MODE::NONE || (s_input.eCurrMode == READ_MODE::NONE && wcsnlen(s_input.aBuff, utils::size(s_input.aBuff)) > 0))
    {
        const String sSearching = ReadModeToString(s_input.eLastUsedMode);
        drawUtf8String(0, height - 1, sSearching, sSearching.size + 2);
        drawWideString(sSearching.size, height - 1, s_input.aBuff, utils::size(s_input.aBuff), width - sSearching.size);

        if (s_input.eCurrMode != READ_MODE::NONE)
        {
            u32 wlen = wcsnlen(s_input.aBuff, utils::size(s_input.aBuff));
            drawWideString(sSearching.size + wlen, height - 1, L"█", 1, 3);
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

static void
drawHelpMenu()
{
    const auto termWidth = tb_width();
    const auto termHeight = tb_height();

    const int x = termWidth/8;
    const int y = termHeight/4;
    const int xWidth = termWidth - (termWidth/4);
    const int yHeight = termHeight/2;

    drawBox(x, y, xWidth, yHeight, TB_YELLOW, TB_DEFAULT, true);
}

void
render(Allocator* pAlloc)
{
    if (tb_height() < 6 || tb_width() < 6) return;

    tb_clear();

    drawStatus(pAlloc);
    drawInfo(pAlloc);

    if (tb_height() > 9 && tb_width() > 9)
    {
        drawTimeSlider();
        drawSongList();
        drawBottomLine();
        if (g_bDrawHelpMenu) drawHelpMenu();
    }

    tb_present();
}

} /* namespace window */
} /* namespace termbox2 */
} /* namespace platform */

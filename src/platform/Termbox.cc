#include "Termbox.hh"

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

static u16 s_firstIdx = 0;
static wchar_t s_aSearchBuff[64] {};
static u32 s_searchBuffIdx = 0;
static bool s_bSearching = false;

/* fix song list range after focus change */
static void
fixFirstIdx()
{
    const auto& pl = *app::g_pPlayer;
    const auto off = pl.statusAndInfoHeight;
    const u16 listHeight = tb_height() - 7 - off;

    const u16 focused = pl.focused;
    u16 first = s_firstIdx;

    if (focused > first + listHeight)
        first = focused - listHeight;
    else if (focused < first)
        first = focused;
    else if (pl.aSongIdxs.size < listHeight)
        first = 0;

    s_firstIdx = first;
}

void
TermboxInit()
{
    [[maybe_unused]] int r = tb_init();
    assert(r == 0 && "'tb_init()' failed");

    tb_set_input_mode(TB_INPUT_ESC|TB_INPUT_MOUSE);

    LOG_NOTIFY("tb_has_truecolor: {}\n", (bool)tb_has_truecolor());
    LOG_NOTIFY("tb_has_egc: {}\n", (bool)tb_has_egc());
}

void
TermboxStop()
{
    tb_shutdown();
    LOG_NOTIFY("tb_shutdown()\n");
}

enum READ_STATUS : u8 { OK, BREAK };

static READ_STATUS
readWChar(tb_event* pEv)
{
    const auto& ev = *pEv;
    tb_peek_event(pEv, defaults::READ_TIMEOUT);

    if (ev.type == 0) return READ_STATUS::BREAK;
    else if (ev.key == TB_KEY_ESC) return READ_STATUS::BREAK;
    else if (ev.key == TB_KEY_CTRL_C) return READ_STATUS::BREAK;
    else if (ev.key == TB_KEY_ENTER) return READ_STATUS::BREAK;
    else if (ev.key == TB_KEY_CTRL_W)
    {
        if (s_searchBuffIdx > 0)
        {
            s_searchBuffIdx = 0;
            utils::fill(s_aSearchBuff, L'\0', utils::size(s_aSearchBuff));
        }
    }
    else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2 || ev.key == TB_KEY_CTRL_H)
    {
        if (s_searchBuffIdx > 0)
            s_aSearchBuff[--s_searchBuffIdx] = L'\0';
    }
    else if (ev.ch)
    {
        if (s_searchBuffIdx < utils::size(s_aSearchBuff) - 1)
            s_aSearchBuff[s_searchBuffIdx++] = ev.ch;
    }

    return READ_STATUS::OK;
}

static void
subStringSearch(Allocator* pAlloc)
{
    tb_event ev {};
    bool bReset = true;

    s_bSearching = true;
    defer( s_bSearching = false );

    auto& pl = *app::g_pPlayer;

    while (true)
    {
        PlayerSubStringSearch(&pl, pAlloc, s_aSearchBuff, utils::size(s_aSearchBuff));
        TermboxRender(pAlloc);

        if (bReset)
        {
            bReset = false;
            utils::fill(s_aSearchBuff, L'\0', utils::size(s_aSearchBuff));
            s_searchBuffIdx = 0;
            TermboxRender(pAlloc);
        }

        auto eRead = readWChar(&ev);
        s_firstIdx = 0;
        pl.focused = s_firstIdx;

        if (eRead == READ_STATUS::BREAK) break;
    }
}

static void
procKey(tb_event* pEv, Allocator* pAlloc)
{
    auto& pl = *app::g_pPlayer;
    auto& mixer = *app::g_pMixer;

    const auto& key = pEv->key;
    const auto& ch = pEv->ch;
    const auto& mod = pEv->mod;

    if (ch == 'q' || ch == L'й')
    {
        app::g_bRunning = false;
        return;
    }
    else if (ch == L'j' || ch == L'о' || key == TB_KEY_ARROW_DOWN)
        PlayerNext(&pl);
    else if (ch == L'k' || ch == L'л' || key == TB_KEY_ARROW_UP)
        PlayerPrev(&pl);
    else if (ch == L'g' || ch == L'п')
        PlayerFocusFirst(&pl);
    else if (ch == L'G' || ch == L'П')
        PlayerFocusLast(&pl);
    else if (key == TB_KEY_CTRL_D)
        PlayerFocus(&pl, pl.focused + 22);
    else if (key == TB_KEY_CTRL_U)
        PlayerFocus(&pl, pl.focused - 22);
    else if (key == TB_KEY_ENTER)
        PlayerSelectFocused(&pl);
    else if (ch == L'/')
        subStringSearch(pAlloc);
    else if (ch == L'z' || ch == L'я')
        PlayerFocusSelected(&pl);
    else if (ch == L' ')
        PlayerTogglePause(&pl);
    else if (ch == L'9')
        mixer.volume = utils::clamp(mixer.volume - 0.1f, 0.0f, defaults::MAX_VOLUME);
    else if (ch == L'(')
        mixer.volume = utils::clamp(mixer.volume - 0.01f, 0.0f, defaults::MAX_VOLUME);
    else if (ch == L'0')
        mixer.volume = utils::clamp(mixer.volume + 0.1f, 0.0f, defaults::MAX_VOLUME);
    else if (ch == L')')
        mixer.volume = utils::clamp(mixer.volume + 0.01f, 0.0f, defaults::MAX_VOLUME);
    else if (ch == L'[')
        audio::MixerChangeSampleRate(&mixer, mixer.changedSampleRate - 1000, false);
    else if (ch == L'{')
        audio::MixerChangeSampleRate(&mixer, mixer.changedSampleRate - 100, false);
    else if (ch == L']')
        audio::MixerChangeSampleRate(&mixer, mixer.changedSampleRate + 1000, false);
    else if (ch == L'}')
        audio::MixerChangeSampleRate(&mixer, mixer.changedSampleRate + 100, false);
    else if (ch == L'\\')
        audio::MixerChangeSampleRate(&mixer, mixer.sampleRate, false);
    else if (ch == L'h' || ch == L'р' || key == TB_KEY_ARROW_LEFT)
    {
        if (mod == TB_MOD_SHIFT) audio::MixerSeekLeftMS(&mixer, 1000);
        else audio::MixerSeekLeftMS(&mixer, 5000);
    }
    else if (ch == L'l' || ch == L'д' || key == TB_KEY_ARROW_RIGHT)
    {
        if (mod == TB_MOD_SHIFT) audio::MixerSeekRightMS(&mixer, 1000);
        else audio::MixerSeekRightMS(&mixer, 5000);
    }
    else if (ch == L'r' || ch == L'к')
        PlayerCycleRepeatMethods(&pl, true);
    else if (ch == L'R' || ch == L'К')
        PlayerCycleRepeatMethods(&pl, false);
    else if (ch == L'm' || ch == L'ь')
        mixer.bMuted = !mixer.bMuted;

    fixFirstIdx();
}

static void
procResize([[maybe_unused]] tb_event* pEv)
{
    //
}

static void
procMouse(tb_event* pEv)
{
    auto& pl = *app::g_pPlayer;
    const long off = pl.statusAndInfoHeight + 4;
    long target = s_firstIdx + pEv->y - off;
    target = utils::clamp(
        target,
        long(s_firstIdx),
        long(s_firstIdx + tb_height() - off - 3)
    );

    defer( fixFirstIdx() );

    const auto& e = *pEv;
    if (e.key == TB_KEY_MOUSE_LEFT)
    {
        if (pl.focused == target)
            PlayerSelectFocused(&pl);
        else PlayerFocus(&pl, target);
    }
    else if (e.key == TB_KEY_MOUSE_WHEEL_UP)
        PlayerFocus(&pl, pl.focused - 22);
    else if (e.key == TB_KEY_MOUSE_WHEEL_DOWN)
        PlayerFocus(&pl, pl.focused + 22);
}

void
TermboxProcEvents(Allocator* pAlloc)
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
        procKey(&ev, pAlloc);
        break;

        case TB_EVENT_RESIZE:
        procResize(&ev);
        break;

        case TB_EVENT_MOUSE:
        procMouse(&ev);
        break;
    }
}

static void
drawBox(
    const u16 x,
    const u16 y,
    const u16 width,
    const u16 height,
    const u32 fgColor,
    const u32 bgColor,
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
)
{
    long it = 0;
    long max = 0;
    while (it < str.size && max < maxLen - 2)
    {
        wchar_t wc {};
        int charLen = mbtowc(&wc, &str[it], str.size - it);
        if (charLen < 0) return;

        it += charLen;

        tb_set_cell(x + 1 + max, y, wc, fg, bg);
        ++max;
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

    for (u16 h = s_firstIdx, i = 0; i < listHeight - 1 && h < pl.aSongIdxs.size; ++h, ++i)
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
    const auto& mixer = *app::g_pMixer;

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
    const auto& mixer = *app::g_pMixer;
    auto& pl = *app::g_pPlayer;

    char* pBuff = (char*)alloc(pAlloc, 1, width);
    utils::fill(pBuff, '\0', width);

    int n = snprintf(pBuff, width, "total: %ld / %u", pl.selected, pl.aShortArgvs.size - 1);
    if (pl.eReapetMethod != PLAYER_REAPEAT_METHOD::NONE)
    {
        const char* sArg {};
        if (pl.eReapetMethod == PLAYER_REAPEAT_METHOD::TRACK) sArg = "track";
        else if (pl.eReapetMethod == PLAYER_REAPEAT_METHOD::PLAYLIST) sArg = "playlist";

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
    int n = 0;

    drawBox(split + 1, 0, tb_width() - split - 2, pl.statusAndInfoHeight + 1, TB_BLUE, TB_DEFAULT);

    char* pBuff = (char*)alloc(pAlloc, 1, width);
    utils::fill(pBuff, '\0', width);

    /* title */
    {
        n = print::toBuffer(pBuff, width, "title: ");
        drawUtf8String(split + 1, 1, pBuff, maxStringWidth);
        utils::fill(pBuff, '\0', width);
        if (pl.info.title.size > 0)
            print::toBuffer(pBuff, width, "{}", pl.info.title);
        else print::toBuffer(pBuff, width, "{}", pl.aShortArgvs[pl.selected]);
        drawUtf8String(split + 1 + n, 1, pBuff, maxStringWidth - n, TB_ITALIC|TB_BOLD|TB_YELLOW);
    }

    /* album */
    {
        utils::fill(pBuff, '\0', width);
        n = print::toBuffer(pBuff, width, "album: ");
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
        n = print::toBuffer(pBuff, width, "artist: ");
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

    if (s_bSearching || (!s_bSearching && wcsnlen(s_aSearchBuff, utils::size(s_aSearchBuff)) > 0))
    {
        String sSearching = "searching: ";
        drawUtf8String(0, height - 1, sSearching, sSearching.size + 2);
        drawWideString(sSearching.size, height - 1, s_aSearchBuff, utils::size(s_aSearchBuff), width - sSearching.size);

        if (s_bSearching)
        {
            u32 wlen = wcsnlen(s_aSearchBuff, utils::size(s_aSearchBuff));
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

    for (int i = xOff + 1; i < width - 1; ++i)
    {
        wchar_t wc = L'━';
        if ((i - 1) == int(timePlace + xOff)) wc = L'╋';
        tb_set_cell(i, off, wc, TB_WHITE, TB_DEFAULT);
    }
}

void
TermboxRender(Allocator* pAlloc)
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
    }

    tb_present();
}

} /* namespace platform */

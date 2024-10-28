#include "Termbox.hh"

#include "adt/defer.hh"
#include "app.hh"
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
    const u16 listHeight = tb_height() - 6 - off;

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
    tb_peek_event(pEv, 5000);

    if (pEv->type == 0) return READ_STATUS::BREAK;
    else if (pEv->key == TB_KEY_ESC) return READ_STATUS::BREAK;
    else if (pEv->key == TB_KEY_CTRL_C) return READ_STATUS::BREAK;
    else if (pEv->key == TB_KEY_ENTER) return READ_STATUS::BREAK;
    else if (pEv->key == TB_KEY_CTRL_W)
    {
        if (s_searchBuffIdx > 0)
        {
            s_searchBuffIdx = 0;
            utils::fill(s_aSearchBuff, L'\0', utils::size(s_aSearchBuff));
        }
    }
    else if (pEv->key == TB_KEY_BACKSPACE || pEv->key == TB_KEY_BACKSPACE2 || pEv->key == TB_KEY_CTRL_H)
    {
        if (s_searchBuffIdx > 0)
            s_aSearchBuff[--s_searchBuffIdx] = L'\0';
    }
    else if (pEv->ch)
    {
        if (s_searchBuffIdx < utils::size(s_aSearchBuff) - 1)
            s_aSearchBuff[s_searchBuffIdx++] = pEv->ch;
    }

    return READ_STATUS::OK;
}

static void
subStringSearch(Allocator* pAlloc)
{
    LOG("subStringSearch\n");

    defer( LOG("subStringSearch leave\n") );

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

    const auto& key = pEv->key;
    const auto& ch = pEv->ch;

    /*LOG("k: {}, ch: '{}'\n", key, (wchar_t)ch);*/

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
        audio::g_globalVolume = utils::clamp(audio::g_globalVolume - 0.1f, 0.0f, 1.0f);
    else if (ch == L'(')
        audio::g_globalVolume = utils::clamp(audio::g_globalVolume - 0.01f, 0.0f, 1.0f);
    else if (ch == L'0')
        audio::g_globalVolume = utils::clamp(audio::g_globalVolume + 0.1f, 0.0f, 1.0f);
    else if (ch == L')')
        audio::g_globalVolume = utils::clamp(audio::g_globalVolume + 0.01f, 0.0f, 1.0f);
    else if (ch == L'[')
        audio::MixerChangeSampleRate(app::g_pMixer, app::g_pMixer->changedSampleRate - 1000, false);
    else if (ch == L']')
        audio::MixerChangeSampleRate(app::g_pMixer, app::g_pMixer->changedSampleRate + 1000, false);
    else if (ch == L'\\')
        audio::MixerChangeSampleRate(app::g_pMixer, app::g_pMixer->sampleRate, false);
    else if (ch == L'h' || ch == L'р')
        audio::MixerSeekLeftMS(app::g_pMixer, 5000);
    else if (ch == L'l' || ch == L'д')
        audio::MixerSeekRightMS(app::g_pMixer, 5000);

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
    const long off = pl.statusAndInfoHeight + 3;
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
    tb_peek_event(&ev, 1000);

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
    /* bop */
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
    const u32 bg = TB_DEFAULT,
    bool bWrapLines = false /* TODO: implement this */
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
    const u32 bg = TB_DEFAULT,
    bool bWrapLines = false /* TODO: implement this */
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
    const u16 listHeight = tb_height() - 4 - off;

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

        drawUtf8String(1, i + off + 3, sSong, maxLen, fg, bg);
    }

    drawBox(0, off + 2, tb_width() - 1, listHeight, TB_BLUE, TB_DEFAULT);
}

static void
drawVolume(Allocator* pAlloc, const u16 split)
{
    const auto width = tb_width();

    char* pBuff = (char*)alloc(pAlloc, 1, width);
    utils::fill(pBuff, '\0', width);
    int n = snprintf(pBuff, width, "volume: %3d", int(std::round(audio::g_globalVolume * 100.0f)));
    drawUtf8String(0, 2, pBuff, split + 1);
    /*for (int i = n + 2, nTimes = 0; i < split && nTimes < maxLine; ++i, ++nTimes)*/
    /*{*/
    /*    tb_set_cell(i, 2, L'▮', TB_GREEN, TB_DEFAULT);*/
    /*}*/
}

static void
drawTime(Allocator* pAlloc, const u16 split)
{
    const auto width = tb_width();
    const auto& mixer = *app::g_pMixer;

    char* pBuff = (char*)alloc(pAlloc, 1, width);
    utils::fill(pBuff, '\0', width);

    u64 t = std::round(mixer.currentTimeStamp / f64(mixer.nChannels) / mixer.changedSampleRate);
    u64 maxT = std::round(mixer.totalSamplesCount / f64(mixer.nChannels) / mixer.changedSampleRate);

    f64 mF = t / 60.0;
    u64 m = u64(mF);
    f64 frac = 60 * (mF - m);

    f64 mFMax = maxT / 60.0;
    u64 mMax = u64(mFMax);
    u64 fracMax = 60 * (mFMax - mMax);

    int n = snprintf(pBuff, width, "time: %lu:%02d / %lu:%02d", m, int(frac), mMax, int(fracMax));

    /*int n = snprintf(pBuff, width, "time: %d:%02d / %d:%02d", currMin, int(currSec), totalMin, int(totalSec));*/
    drawUtf8String(0, 1, pBuff, split + 1);
}

static void
drawStatus(Allocator* pAlloc)
{
    const auto width = tb_width();
    const auto& pl = *app::g_pPlayer;
    const u16 split = std::round(f64(width) * 0.4);

    drawBox(0, 0, split, pl.statusAndInfoHeight + 1, TB_WHITE, TB_DEFAULT);

    drawTime(pAlloc, split);
    drawVolume(pAlloc, split);
}

static void
drawInfo()
{
    const auto& pl = *app::g_pPlayer;
    const u16 split = std::round(f64(tb_width()) * pl.statusToInfoWidthRatio);

    drawBox(split + 1, 0, tb_width() - split - 2, pl.statusAndInfoHeight + 1, TB_BLUE, TB_DEFAULT);
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

void
TermboxRender([[maybe_unused]] Allocator* pAlloc)
{
    if (tb_height() < 6 || tb_width() < 6) return;

    tb_clear();

    /*LOG("focused: {}, selected: {}\n", app::g_pPlayer->focused, app::g_pPlayer->selected);*/

    drawStatus(pAlloc);
    drawInfo();

    if (tb_height() > 9 && tb_width() > 9)
    {
        drawSongList();
        drawBottomLine();
    }

    tb_present();
}

} /* namespace platform */

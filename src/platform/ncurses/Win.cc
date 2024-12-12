#include "Win.hh"

#include "adt/defer.hh"
#include "adt/enum.hh"
#include "common.hh"
#include "defaults.hh"
#include "input.hh"
#include "keys.hh"

#include <clocale>
#include <cmath>
#include <ncursesw/ncurses.h>
#include <stdatomic.h>

#ifdef USE_CHAFA
    #include "platform/chafa/chafa.hh"
#endif

namespace platform
{
namespace ncurses
{

enum class COLOR : short
{
    TERMDEF = -1, /* -1 should preserve terminal default color when use_default_colors() */
    BLACK = 0,
    WHITE = 1,
    GREEN,
    YELLOW,
    BLUE,
    CYAN,
    RED
};
ADT_ENUM_BITWISE_OPERATORS(COLOR);

static void WinCreateBoxes(Win* s);
static void WinDestroyBoxes(Win* s);
static void drawBox(WINDOW* pWin, COLOR eColor);
static void WinProcResize(Win* s);
static void WinDrawList(Win* s);

int g_timeStringSize {};

bool
WinStart(Win* s, Arena* pArena)
{
    s->pArena = pArena;

    initscr();
    start_color();
    use_default_colors();
    curs_set(0);
    set_escdelay(0);
    timeout(defaults::UPDATE_RATE);
    noecho();
    /*raw();*/
    cbreak();
    keypad(stdscr, true);

    mousemask(ALL_MOUSE_EVENTS, {});

    short td =  -1;
    init_pair(short(COLOR::GREEN), COLOR_GREEN, td);
    init_pair(short(COLOR::YELLOW), COLOR_YELLOW, td);
    init_pair(short(COLOR::BLUE), COLOR_BLUE, td);
    init_pair(short(COLOR::CYAN), COLOR_CYAN, td);
    init_pair(short(COLOR::RED), COLOR_RED, td);
    init_pair(short(COLOR::WHITE), COLOR_WHITE, td);
    init_pair(short(COLOR::BLACK), COLOR_BLACK, td);

    WinCreateBoxes(s);

    refresh();

    return true;
}

void
WinDestroy(Win* s)
{
    WinDestroyBoxes(s);
    endwin();
}

void
WinProcEvents(Win* s)
{
    wint_t ch {};
    get_wch(&ch);

    if (ch == KEY_RESIZE) WinProcResize(s);
    else if (ch == KEY_MOUSE)
    {
        MEVENT ev;
        getmouse(&ev);
        input::WinProcMouse(s, ev);
        common::fixFirstIdx(getmaxy(s->list.pCon) - 1, &s->firstIdx);
    }
    else if (ch != 0)
    {
        input::WinProcKey(s, ch);
        common::fixFirstIdx(getmaxy(s->list.pCon) - 1, &s->firstIdx);
    }
}

static common::READ_STATUS
readWChar()
{
    timeout(defaults::READ_TIMEOUT);
    defer( timeout(defaults::UPDATE_RATE) );

    wint_t ch {};
    wget_wch(stdscr, &ch);

    namespace c = common;

    if (ch == keys::ESC) return c::READ_STATUS::DONE;
    else if (ch == keys::ENTER) return c::READ_STATUS::DONE;
    else if (ch == keys::CTRL_W)
    {
        if (c::g_input.idx > 0)
        {
            c::g_input.idx = 0;
            c::g_input.zeroOutBuff();
        }
    }
    else if (ch == KEY_BACKSPACE)
    {
        if (c::g_input.idx > 0)
            c::g_input.aBuff[--c::g_input.idx] = L'\0';
    }
    else if (ch > 0)
    {
        if (c::g_input.idx < utils::size(c::g_input.aBuff) - 1)
            c::g_input.aBuff[c::g_input.idx++] = ch;
    }

    return c::READ_STATUS::OK_;
}

void
WinSeekFromInput(Win* s)
{
    common::seekFromInput(
        +[](void*) { return readWChar(); },
        nullptr,
        +[](void* pArg) { WinDraw((Win*)pArg); },
        s
    );
}

void
WinSubStringSearch(Win* s)
{
    common::subStringSearch(
        s->pArena,
        +[](void*) { return readWChar(); },
        nullptr,
        +[](void* pArg) { WinDraw((Win*)pArg); },
        s,
        &s->firstIdx
    );
}

static void
WinCreateBoxes(Win* s)
{
    int height, width;
    getmaxyx(stdscr, height, width);
    auto& pl = *app::g_pPlayer;

    f64 split = f64(height) * pl.statusToInfoWidthRatio;
    /*f64 split = f64(mx) * 0.4;*/

    s->info.pBor = subwin(stdscr, std::round(f64(height) - split), width, 0, 0);
    s->info.pCon = derwin(s->info.pBor, getmaxy(s->info.pBor) - 2, getmaxx(s->info.pBor) - 2, 1, 1);

    s->list.pBor = subwin(stdscr, std::round(split - 1.0) - 2, width, std::round(f64(height) - split) + 2, 0);
    s->list.pCon = derwin(s->list.pBor, getmaxy(s->list.pBor) - 2, width - 2, 1, 1);

    s->split = split;
}

static void
WinDestroyBoxes(Win* s)
{
    delwin(s->info.pBor);
    delwin(s->info.pCon);

    delwin(s->list.pBor);
    delwin(s->list.pCon);

    s->info.pBor = s->info.pCon = nullptr;
    s->list.pBor = s->list.pCon = nullptr;
}

static void
drawBox(WINDOW* pWin, COLOR eColor)
{
    cchar_t ls, rs, ts, bs, tl, tr, bl, br;
    short col = short(eColor);

    setcchar(&ls, L"┃", 0, col, {});
    setcchar(&rs, L"┃", 0, col, {});
    setcchar(&ts, L"━", 0, col, {});
    setcchar(&bs, L"━", 0, col, {});
    setcchar(&tl, L"┏", 0, col, {});
    setcchar(&tr, L"┓", 0, col, {});
    setcchar(&bl, L"┗", 0, col, {});
    setcchar(&br, L"┛", 0, col, {});
    wborder_set(pWin, &ls, &rs, &ts, &bs, &tl, &tr, &bl, &br);
}

static void
WinProcResize(Win* s)
{
    WinDestroyBoxes(s);
    WinCreateBoxes(s);

    redrawwin(stdscr);
    werase(stdscr);
    refresh();

    app::g_pPlayer->bSelectionChanged = true;
    WinDraw(s);
}

static void
WinDrawList(Win* s)
{
    const auto& pl = *app::g_pPlayer;

    wclear(s->list.pCon);
    drawBox(s->list.pBor, COLOR::BLUE);

    for (u16 h = s->firstIdx, i = 0; h < pl.aSongIdxs.size; ++h, ++i)
    {
        const u16 songIdx = pl.aSongIdxs[h];
        const String sSong = pl.aShortArgvs[songIdx];

        bool bSelected = songIdx == pl.selected ? true : false;

        int col = COLOR_PAIR(COLOR::WHITE);
        wattron(s->list.pCon, col);

        if (h == pl.focused && bSelected)
            col = COLOR_PAIR(COLOR::YELLOW) | A_BOLD | A_REVERSE;
        else if (h == pl.focused)
            col |= A_REVERSE;
        else if (bSelected)
            col = COLOR_PAIR(COLOR::YELLOW) | A_BOLD;

        int mx = getmaxx(s->list.pCon);

        auto* pWstr = (wchar_t*)zalloc(s->pArena, sizeof(wchar_t), mx + 1);
        mbstowcs(pWstr, sSong.pData, sSong.size);

        wattron(s->list.pCon, col);
        mvwaddnwstr(s->list.pCon, i, 0, pWstr, mx);
        wattroff(s->list.pCon, col | A_REVERSE | A_BOLD);
    }
}

static void
WinDrawBottomLine(Win* s)
{
    namespace c = common;

    const auto& pl = *app::g_pPlayer;
    int height, width;
    getmaxyx(stdscr, height, width);

    move(height - 1, 0);
    clrtoeol();

    /* selected / focused */
    {
        char* pBuff = (char*)zalloc(s->pArena, 1, width + 1);

        int n = print::toBuffer(pBuff, width, "selected: {} / {}", pl.selected, pl.aShortArgvs.size - 1);
        if (pl.eReapetMethod != PLAYER_REPEAT_METHOD::NONE)
        {
            const char* sArg {};
            if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::TRACK) sArg = "track";
            else if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST) sArg = "playlist";

            n += print::toBuffer(pBuff + n, width - n, " (repeat {})", sArg);
        }

        n += print::toBuffer(pBuff + n, width - 1, " | focused: {}", pl.focused);
        mvwaddnstr(stdscr, height - 1, width - n - 2, pBuff, n);
    }

    if (
        c::g_input.eCurrMode != READ_MODE::NONE ||
        (c::g_input.eCurrMode == READ_MODE::NONE && wcsnlen(c::g_input.aBuff, utils::size(c::g_input.aBuff)) > 0)
    )
    {
        const String sReadMode = c::readModeToString(c::g_input.eLastUsedMode);
        mvwaddnwstr(stdscr, height - 1, sReadMode.size + 1, c::g_input.aBuff, utils::size(c::g_input.aBuff));
        mvwaddnstr(stdscr, height - 1, 1, sReadMode.pData, sReadMode.size);

        if (c::g_input.eCurrMode != READ_MODE::NONE)
        {
            u32 wlen = wcsnlen(c::g_input.aBuff, utils::size(c::g_input.aBuff));
            mvwaddnwstr(stdscr, height - 1, sReadMode.size + wlen + 1, common::CURSOR_BLOCK, 1);
        }
    }
}

static void
WinDrawInfo(Win* s)
{
    const auto& pl = *app::g_pPlayer;
    auto* pWin = s->info.pCon;

    int width = getmaxx(pWin);
    char* pBuff = (char*)zalloc(s->pArena, 1, width + 1);

    /* title */
    {
        int n = print::toBuffer(pBuff, width, "title: ");
        mvwaddnstr(pWin, 0, 0, pBuff, width);

        memset(pBuff, 0, width + 1);
        if (pl.info.title.size > 0) print::toBuffer(pBuff, width, "{}", pl.info.title);
        else print::toBuffer(pBuff, width, "{}", pl.aShortArgvs[pl.selected]);

        int col = A_BOLD | A_ITALIC | COLOR_PAIR(COLOR::YELLOW);
        wattron(pWin, col);
        mvwaddnstr(pWin, 0, n, pBuff, width);
        wattroff(pWin, col);
    }

    /* album */
    {
        memset(pBuff, 0, width + 1);
        int n = print::toBuffer(pBuff, width, "album: ");

        mvwaddnstr(pWin, 1, 0, pBuff, width);
        if (pl.info.album.size > 0)
        {
            memset(pBuff, 0, width + 1);
            print::toBuffer(pBuff, width, "{}", pl.info.album);

            mvwaddnstr(pWin, 1, n, pBuff, width);
        }
    }

    /* artist */
    {
        memset(pBuff, 0, width + 1);
        int n = print::toBuffer(pBuff, width, "artist: ");

        mvwaddnstr(pWin, 2, 0, pBuff, width);
        if (pl.info.artist.size > 0)
        {
            memset(pBuff, 0, width + 1);
            print::toBuffer(pBuff, width, "{}", pl.info.artist);

            wattron(pWin, A_BOLD);
            mvwaddnstr(pWin, 2, n, pBuff, width);
            wattroff(pWin, A_BOLD);
        }
    }
}

static void
WinDrawTime(Win* s)
{
    const auto& mix = *app::g_pMixer;
    auto* pWin = stdscr;

    int height, width;
    getmaxyx(pWin, height, width);

    int split = std::round(f64(height) * (1.0 - app::g_pPlayer->statusToInfoWidthRatio));
    int n = 0;

    move(split, 0);
    clrtoeol();

    /* time */
    {
        const String str = common::allocTimeString(s->pArena, width);
        mvwaddnstr(pWin, split, 1, str.pData, str.size);
        n += str.size;
    }

    /* play/pause indicator */
    {
        bool bPaused = atomic_load_explicit(&mix.bPaused, memory_order_relaxed);
        const char* ntsIndicator = bPaused ? "II" : "I>";

        wattron(pWin, A_BOLD);
        mvwaddstr(pWin, split, 2 + n, ntsIndicator);
        wattroff(pWin, A_BOLD);

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
            cchar_t wch {};
            wchar_t aCh[2] {L'━', L'\0'};
            int pair = 0;
            int fg = 200, bg = 0;

            if ((t - 0) == std::floor(timePlace)) aCh[0] = L'╋';

            init_extended_pair(pair, fg, bg);
            setcchar(&wch, aCh, A_NORMAL, -1, &pair);

            mvwadd_wch(pWin, split, i, &wch);
        }
    }
}

static void
WinDrawVolume(Win* s)
{
    auto* pWin = stdscr;

    int height = getmaxy(pWin);
    int width = getmaxx(pWin) - 1;
    int split = std::round(f64(height) * (1.0 - app::g_pPlayer->statusToInfoWidthRatio)) + 1;

    move(split, 0);
    clrtoeol();

    using VOLUME_COLOR = int;

    const f32 vol = app::g_pMixer->volume;
    const bool bMuted = app::g_pMixer->bMuted;
    constexpr String fmt = "volume: %3d";
    const int maxVolumeBars = (width - fmt.size - 2) * vol * 1.0f/defaults::MAX_VOLUME;

    auto volumeColor = [&](int i) -> VOLUME_COLOR {
        f32 col = f32(i) / (f32(width - fmt.size - 2 - 1) * (1.0f/defaults::MAX_VOLUME));

        if (col <= 0.33f) return COLOR_PAIR(COLOR::GREEN);
        else if (col > 0.33f && col <= 0.66f) return COLOR_PAIR(COLOR::GREEN);
        else if (col > 0.66f && col <= 1.00f) return COLOR_PAIR(COLOR::YELLOW);
        else return COLOR_PAIR(COLOR::RED);
    };

    const auto col = bMuted ? COLOR_PAIR(COLOR::BLUE) : volumeColor(maxVolumeBars - 1) | A_BOLD;

    for (int i = fmt.size + 2, nTimes = 0; i < width && nTimes < maxVolumeBars; ++i, ++nTimes)
    {
        VOLUME_COLOR col = A_BOLD;
        chtype wc;
        if (bMuted)
        {
            col |= COLOR_PAIR(COLOR::BLUE);
            wc = ACS_S7;
        }
        else
        {
            col |= volumeColor(nTimes);
            wc = ACS_DIAMOND;
        }

        attron(col);
        mvwaddch(pWin, split, i, wc);
    }

    char* pBuff = (char*)zalloc(s->pArena, 1, width + 1);
    snprintf(pBuff, width, fmt.pData, int(std::round(vol * 100.0f)));
    mvwaddnstr(pWin, split, 1, pBuff, width);
    attroff(col | A_BOLD);
}

void
WinDraw(Win* s)
{
    int y, x;
    getmaxyx(stdscr, y, x);

    if (y < 10 || x < 10) return;

    if (app::g_pPlayer->bSelectionChanged)
    {
        app::g_pPlayer->bSelectionChanged = false;

        wclear(s->info.pCon);

#ifdef USE_CHAFA
        auto oCover = audio::MixerGetCoverImage(app::g_pMixer);
        if (oCover)
        {
            const auto& img = oCover.data;
            int y, x;
            getmaxyx(s->info.pCon, y, x);

            platform::chafa::showImageNCurses(s->info.pCon, img, y + 1, x);
        }
#endif

        WinDrawInfo(s);
    }

    WinDrawList(s);
    WinDrawBottomLine(s); /* draws to the stdscr */
    WinDrawTime(s);
    WinDrawVolume(s);
    drawBox(s->info.pBor, COLOR::BLUE);

    wnoutrefresh(stdscr);
    wnoutrefresh(s->info.pCon);
    wnoutrefresh(s->info.pBor);
    wnoutrefresh(s->list.pBor);

    doupdate();
}

} /*namespace ncurses */
} /*namespace platform */

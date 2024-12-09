#include "Win.hh"

#include "adt/logs.hh"
#include "defaults.hh"
#include "input.hh"
#include "common.hh"
#include "adt/enum.hh"

#include <clocale>
#include <ncursesw/ncurses.h>

#include <cmath>

#include "platform/chafa/Img.hh"

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
static void drawList(Win* s);

static struct {
    wchar_t aBuff[64] {};
    u32 idx = 0;
    READ_MODE eCurrMode {};
    READ_MODE eLastUsedMode {};

    void zeroOutBuff() { memset(aBuff, 0, sizeof(aBuff)); }
} s_input {};

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
WinDraw(Win* s)
{
    drawBox(s->info.pBor, COLOR::BLUE);
    
    drawList(s);

    if (app::g_pPlayer->bSelectionChanged)
    {
        app::g_pPlayer->bSelectionChanged = false;

        auto oCover = audio::MixerGetCoverImage(app::g_pMixer);
        if (oCover)
        {
            int y, x;
            getmaxyx(s->info.pCon, y, x);
            platform::chafa::showImage(s->info.pCon, oCover.data, y, x);
        }
    }

    wnoutrefresh(s->info.pBor);
    wnoutrefresh(s->list.pBor);

    doupdate();
}

void
WinProcEvents(Win* s)
{
    wint_t ch {};
    get_wch(&ch);

    if (ch == KEY_RESIZE) WinProcResize(s);
    else if (ch != 0)
    {
        input::procKey(s, ch);
        common::fixFirstIdx(getmaxy(s->list.pCon) - 1, &s->firstIdx);
    }
}

void
WinSeekFromInput(Win* s)
{
}

void
WinSubStringSearch(Win* s)
{
    LOG("subStringSearch()\n");

    echo();
    char aBuff[64] {};
    getnstr(aBuff, sizeof(aBuff));

    noecho();
}

static void
WinCreateBoxes(Win* s)
{
    int mx, my;
    getmaxyx(stdscr, mx, my);
    auto& pl = *app::g_pPlayer;

    f64 split = f64(mx) * pl.statusToInfoWidthRatio;

    s->info.pBor = subwin(stdscr, std::round(f64(mx) - split), my, 0, 0);
    s->info.pCon = derwin(s->info.pBor, getmaxy(s->info.pBor) - 2, getmaxx(s->info.pBor) - 2, 1, 1);

    s->list.pBor = subwin(stdscr, std::round(split - 1.0), my, std::round(f64(mx) - split), 0);
    s->list.pCon = derwin(s->list.pBor, getmaxy(s->list.pBor) - 2, my - 2, 1, 1);

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
    LOG_WARN("REFRESH\n");

    app::g_pPlayer->bSelectionChanged = true;
    WinDraw(s);
}

static void
drawList(Win* s)
{
    const auto& pl = *app::g_pPlayer;
    const int off = std::round(s->split);

    wclear(s->list.pCon);

    for (u16 h = s->firstIdx, i = 0; h < pl.aSongIdxs.size; ++h, ++i)
    {
        const u16 songIdx = pl.aSongIdxs[h];
        const String sSong = pl.aShortArgvs[songIdx];
        const u32 maxLen = getmaxx(s->list.pCon);

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

        // for (long len = 0; len < mx; ++len)
        // {
        //     if (!pWstr[len]) break;

        //     int pair = 0;
        //     cchar_t cch;
        //     wchar_t wc[2] {pWstr[len], L'\0'};

        //     setcchar(&cch, wc, A_NORMAL, -1, &pair);
        //     mvwadd_wch(s->list.pCon, i, len, &cch);
        // }

        wattroff(s->list.pCon, col | A_REVERSE | A_BOLD);
    }

    drawBox(s->list.pBor, COLOR::BLUE);
}

} /*namespace ncurses */
} /*namespace platform */

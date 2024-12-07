#include "Win.hh"

#include "defaults.hh"
#include "input.hh"

#include <ncurses.h>

namespace platform
{
namespace ncurses
{

bool
WinStart(Win* s, Arena* pArena)
{
    initscr();
    start_color();
    use_default_colors();
    curs_set(0);
    set_escdelay(0);
    timeout(defaults::UPDATE_RATE);
    noecho();
    cbreak();
    keypad(stdscr, true);

    refresh();

    return true;
}

void
WinDestroy(Win* s)
{
    endwin();
}

void
WinDraw(Win* s)
{
}

void
WinProcEvents(Win* s)
{
    input::procEvents();
}

void
WinSeekFromInput(Win* s)
{
}

void
WinSubStringSearch(Win* s)
{
}

} /*namespace ncurses */
} /*namespace platform */

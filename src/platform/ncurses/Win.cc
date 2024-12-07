#include "Win.hh"

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

    curs_set(0);
    set_escdelay(0);
    noecho();
    cbreak();

    keypad(stdscr, true);
    refresh();

    return true;
}

void
WinDestroy(Win* s)
{
}

void
WinDraw(Win* s)
{
}

void
WinProcEvents(Win* s)
{
}

} /*namespace ncurses */
} /*namespace platform */

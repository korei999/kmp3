#include "input.hh"

#include "adt/logs.hh"
#include "app.hh"

#include <ncurses.h>

namespace platform
{
namespace ncurses
{
namespace input
{

void
procEvents()
{
    int ch {};
    while (app::g_bRunning)
    {
        ch = getch();
        LOG_GOOD("ch({}): '{}'\n", ch, (wchar_t)ch);
    }
}

} /*namespace input */
} /*namespace ncurses */
} /*namespace platform */

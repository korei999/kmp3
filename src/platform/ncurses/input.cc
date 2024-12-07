#include "input.hh"

#include "adt/logs.hh"
#include "app.hh"
#include "keys.hh"

#include <ncurses.h>

namespace platform
{
namespace ncurses
{
namespace input
{

static u16 s_aInputMap[1000] {};

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

void
fillInputMap()
{
    for (int i = '!'; i < '~'; ++i) s_aInputMap[i] = i;
}

} /*namespace input */
} /*namespace ncurses */
} /*namespace platform */

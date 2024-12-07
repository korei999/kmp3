#include "input.hh"

#include "app.hh"
#include "keybinds.hh"

#include <ncurses.h>

namespace platform
{
namespace ncurses
{
namespace input
{

static void
procKey(int ch)
{
    if (ch == -1) return;

    for (auto& k : keybinds::gc_aKeys)
    {
        auto& pfn = k.pfn;
        auto& arg = k.arg;

        if ((k.key > 0 && k.key == ch) || (k.ch > 0 && k.ch == (u32)ch))
            resolvePFN(k.pfn, k.arg);
    }
}

void
procEvents()
{
    while (app::g_bRunning)
    {
        int ch = getch();

        procKey(ch);
    }
}

} /*namespace input */
} /*namespace ncurses */
} /*namespace platform */

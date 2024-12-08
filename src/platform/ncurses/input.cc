#include "input.hh"

#include "adt/logs.hh"
#include "keybinds.hh"

#include <ncurses.h>

namespace platform
{
namespace ncurses
{
namespace input
{

void
procKey(Win* s, wint_t ch)
{
    LOG_GOOD("ch({}): '{}'\n", ch, (wchar_t)ch);

    for (const auto& k : keybinds::inl_aKeys)
    {
        auto& pfn = k.pfn;
        auto& arg = k.arg;

        if ((k.key > 0 && k.key == ch) || (k.ch > 0 && k.ch == (u32)ch))
            resolvePFN(k.pfn, k.arg);
    }
}

} /*namespace input */
} /*namespace ncurses */
} /*namespace platform */

#include "input.hh"

#include "keybinds.hh"

#include <ncurses.h>

namespace platform
{
namespace ncurses
{
namespace input
{

void
WinProcKey([[maybe_unused]] Win* s, wint_t ch)
{
    for (const auto& k : keybinds::inl_aKeys)
    {
        if ((k.key > 0 && k.key == ch) || (k.ch > 0 && k.ch == (u32)ch))
            keybinds::resolveKey(k.pfn, k.arg);
    }
}

} /*namespace input */
} /*namespace ncurses */
} /*namespace platform */

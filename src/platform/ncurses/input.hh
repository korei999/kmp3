#pragma once

#include "Win.hh"

namespace platform
{
namespace ncurses
{
namespace input
{

void WinProcKey(Win* s, wint_t ch);
void WinProcMouse(Win* s, MEVENT ev);

} /*namespace input */
} /*namespace ncurses */
} /*namespace platform */

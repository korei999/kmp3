#pragma once

#include "IWindow.hh"

#include <notcurses/notcurses.h>

namespace platform
{
namespace notcurses
{

struct Win
{
    IWindow super {};
    Arena* pArena {};
    struct notcurses* pNC {};

    Win();
};

bool WinStart(Win* s, Arena* pArena);
void WinDestroy(Win* s);
void WinDraw(Win* s);
void WinProcEvents(Win* s);
void WinSeekFromInput(Win* s);
void WinSubStringSearch(Win* s);

inline const WindowVTable inl_WinVTable {
    .start = decltype(WindowVTable::start)(WinStart),
    .destroy = decltype(WindowVTable::destroy)(WinDestroy),
    .draw = decltype(WindowVTable::draw)(WinDraw),
    .procEvents = decltype(WindowVTable::procEvents)(WinProcEvents),
    .seekFromInput = decltype(WindowVTable::seekFromInput)(WinSeekFromInput),
    .subStringSearch = decltype(WindowVTable::subStringSearch)(WinSubStringSearch),
};

inline
Win::Win() : super(&inl_WinVTable) {}

} /* namespace notcurses */
} /* namespace platform */

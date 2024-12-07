#pragma once

#include "IWindow.hh"

using namespace adt;

namespace platform
{
namespace ncurses
{

struct Win
{
    IWindow super {};

    Win();
};

bool WinStart(Win* s, Arena* pArena);
void WinDestroy(Win* s);
void WinDraw(Win* s);
void WinProcEvents(Win* s);

inline const WindowVTable inl_WinVTable {
    .start = decltype(WindowVTable::start)(WinStart),
    .destroy = decltype(WindowVTable::destroy)(WinDestroy),
    .draw = decltype(WindowVTable::draw)(WinDraw),
    .procEvents = decltype(WindowVTable::procEvents)(WinProcEvents),
};

inline
Win::Win() : super(&inl_WinVTable) {}

} /*namespace ncurses */
} /*namespace platform */

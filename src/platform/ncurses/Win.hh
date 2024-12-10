#pragma once

#include "IWindow.hh"

#include <ncurses.h>

using namespace adt;

namespace platform
{
namespace ncurses
{

extern int g_timeStringSize;

struct Box
{
    WINDOW* pBor {}; /* border window */
    WINDOW* pCon {}; /* content window */
};

struct Win
{
    IWindow super {};
    Arena* pArena {};
    Box list {};
    Box info {};
    f64 split {};
    u16 firstIdx {};

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

} /*namespace ncurses */
} /*namespace platform */

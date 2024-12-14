#pragma once

#include "IWindow.hh"
#include "TextBuff.hh"

namespace platform
{
namespace ansi
{

struct Win
{
    IWindow super {};
    Arena* pArena {};
    TextBuff textBuff {};

    Win();
};

bool WinStart(Win* s, Arena* pArena);
void WinDestroy(Win* s);
void WinDraw(Win* s);
inline void WinProcEvents(Win* s) { /* noop */ }
void WinSeekFromInput(Win* s);
void WinSubStringSearch(Win* s);

inline const WindowVTable inl_WinWindowVTable {
    .start = decltype(WindowVTable::start)(WinStart),
    .destroy = decltype(WindowVTable::destroy)(WinDestroy),
    .draw = decltype(WindowVTable::draw)(WinDraw),
    .procEvents = decltype(WindowVTable::procEvents)(WinProcEvents),
    .seekFromInput = decltype(WindowVTable::seekFromInput)(WinSeekFromInput),
    .subStringSearch = decltype(WindowVTable::subStringSearch)(WinSubStringSearch),
};

inline
Win::Win() : super(&inl_WinWindowVTable) {}

} /* namespace ansi */
} /* namespace platform */

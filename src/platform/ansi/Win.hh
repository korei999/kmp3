#pragma once

#include "IWindow.hh"
#include "TextBuff.hh"
#include "TermSize.hh"

#include <termios.h>

namespace platform
{
namespace ansi
{

extern TermSize g_termSize;

struct Win
{
    IWindow super {};
    Arena* pArena {};
    TextBuff textBuff {};
    termios termOg {};
    u16 firstIdx {};

    Win();
};

bool WinStart(Win* s, Arena* pArena);
void WinDestroy(Win* s);
void WinDraw(Win* s);
void WinProcEvents(Win* s);
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

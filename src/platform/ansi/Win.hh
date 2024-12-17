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

    bool start(Arena* pArena);
    void destroy();
    void draw();
    void procEvents();
    void seekFromInput();
    void subStringSearch();
};

inline const WindowVTable inl_WinWindowVTable {
    .start = decltype(WindowVTable::start)(+[](Win* s, Arena* pArena) { return s->start(pArena); }),
    .destroy = decltype(WindowVTable::destroy)(+[](Win* s) { s->destroy(); }),
    .draw = decltype(WindowVTable::draw)(+[](Win* s) { s->draw(); }),
    .procEvents = decltype(WindowVTable::procEvents)(+[](Win* s) { s->procEvents(); }),
    .seekFromInput = decltype(WindowVTable::seekFromInput)(+[](Win* s) { s->seekFromInput(); }),
    .subStringSearch = decltype(WindowVTable::subStringSearch)(+[](Win* s) { s->subStringSearch(); }),
};

inline
Win::Win() : super(&inl_WinWindowVTable) {}

} /* namespace ansi */
} /* namespace platform */

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
    bool bRedraw = true;

    Win();

    bool start(Arena* pArena);
    void destroy();
    void draw();
    void procEvents();
    void seekFromInput();
    void subStringSearch();
};

inline const WindowVTable inl_WinWindowVTable = WindowVTableGenerate<Win>();

inline
Win::Win() : super(&inl_WinWindowVTable) {}

} /* namespace ansi */
} /* namespace platform */

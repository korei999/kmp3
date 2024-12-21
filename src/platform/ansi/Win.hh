#pragma once

#include "IWindow.hh"
#include "TextBuff.hh"
#include "TermSize.hh"

#include <termios.h>
#include <threads.h>

namespace platform
{
namespace ansi
{

extern TermSize g_termSize;

struct Win : public IWindow
{
    Arena* pArena {};
    TextBuff textBuff {};
    termios termOg {};
    u16 firstIdx {};
    bool bRedraw = true;
    bool bClear = false;
    int prevImgWidth = 0;
    mtx_t mtxUpdate {};
    f64 time {};
    f64 lastResizeTime {};

    /* */

public:
    virtual bool start(Arena* pArena) final;
    virtual void destroy() final;
    virtual void draw() final;
    virtual void procEvents() final;
    virtual void seekFromInput() final;
    virtual void subStringSearch() final;
};

} /* namespace ansi */
} /* namespace platform */

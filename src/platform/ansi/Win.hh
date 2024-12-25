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
    Arena* m_pArena {};
    TextBuff m_textBuff {};
    termios m_termOg {};
    u16 m_firstIdx {};
    bool m_bRedraw = true;
    bool m_bClear = false;
    int m_prevImgWidth = 0;
    mtx_t m_mtxUpdate {};
    f64 m_time {};
    f64 m_lastResizeTime {};

    /* */

public:
    virtual bool start(Arena* pArena) final;
    virtual void destroy() final;
    virtual void draw() final;
    virtual void procEvents() final;
    virtual void seekFromInput() final;
    virtual void subStringSearch() final;

    /* */

private:
    void disableRawMode();
    void enableRawMode();
};

} /* namespace ansi */
} /* namespace platform */

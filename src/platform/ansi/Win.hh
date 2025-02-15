#pragma once

#include "adt/Thread.hh"

#include "IWindow.hh"
#include "TextBuff.hh"
#include "TermSize.hh"
#include "common.hh"

#include <termios.h>

namespace platform::ansi
{

extern TermSize g_termSize;

class Win : public IWindow
{
    Arena* m_pArena {};
    TextBuff m_textBuff {};
    termios m_termOg {};
    i16 m_firstIdx {};
    int m_prevImgWidth = 0;
    Mutex m_mtxUpdate {};
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
    virtual void centerAroundSelection() final;
    virtual void adjustListHeight() final;

    /* */

private:
    void disableRawMode();
    void enableRawMode();

    /* draw */
    void clearArea(int x, int y, int width, int height);

#ifdef USE_CHAFA
    void coverImage();
#endif

    void info();
    void volume();
    void time();
    void timeSlider();
    void list();
    void bottomLine();
    void update();
    /* */

    /* input */
    void procInput();
    common::READ_STATUS readWChar();
    /* */

    friend void sigwinchHandler(int sig);
};

} /* namespace platform::ansi */

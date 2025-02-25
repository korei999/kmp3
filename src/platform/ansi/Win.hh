#pragma once

#include "adt/Thread.hh"

#include "IWindow.hh"
#include "TextBuff.hh"
#include "TermSize.hh"
#include "common.hh"

#include <termios.h>

namespace platform::ansi
{

class Win : public IWindow
{
    struct MouseInput
    {
        enum class KEY : u8 { NONE, WHEEL_UP, WHEEL_DOWN, LEFT, MIDDLE, RIGHT, RELEASE };

        /* */

        KEY eKey {};
        int x {};
        int y {};

        /* */

        friend bool operator==(MouseInput a, MouseInput b)
        {
            return a.eKey == b.eKey && a.x == b.x && a.y == b.y;
        }
    };

    struct Input
    {
        enum class TYPE : u8 { KB, MOUSE };

        /* */

        MouseInput mouse {};
        int key {};
        TYPE eType {};
    };

    static constexpr MouseInput INVALID_MOUSE {.eKey = MouseInput::KEY::NONE, .x = -1, .y = -1};

    /* */

    Arena* m_pArena {};
    TextBuff m_textBuff {};
    termios m_termOg {};
    i16 m_firstIdx {};
    int m_prevImgWidth = 0;
    Mutex m_mtxUpdate {};
    f64 m_time {};
    f64 m_lastResizeTime {};
    Input m_lastInput {};
    int m_lastMouseSelection {};
    f64 m_lastMouseSelectionTime {};

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

#ifdef OPT_CHAFA
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

private:
    Input readFromStdin(const int timeoutMS);
    [[nodiscard]] ADT_NO_UB int parseSeq(Span<char> spBuff, ssize_t nRead);
    [[nodiscard]] ADT_NO_UB MouseInput parseMouse(Span<char> spBuff, ssize_t nRead);
    void procMouse(MouseInput in);
};

extern TermSize g_termSize;

} /* namespace platform::ansi */

#pragma once

#include "IWindow.hh"
#include "TextBuff.hh"
#include "TermSize.hh"
#include "common-inl.hh"

#include <termios.h>

namespace platform::ansi
{

class Win : public IWindow
{
protected:
    struct MouseInput
    {
        enum class KEY : adt::u8 { NONE, WHEEL_UP, WHEEL_DOWN, LEFT, MIDDLE, RIGHT, RELEASE };

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
        enum class TYPE : adt::u8 { KB, MOUSE, TIMEOUT };

        /* */

        MouseInput mouse {};
        int key {};
        TYPE eType {};
    };

    static constexpr MouseInput INVALID_MOUSE {.eKey = MouseInput::KEY::NONE, .x = -1, .y = -1};

    /* */

    adt::Arena* m_pArena {};
    TextBuff m_textBuff {};
    termios m_termOg {};
    TermSize m_termSize {};
    int m_prevImgWidth = 0;
    adt::Mutex m_mtxUpdate {};
    adt::i64 m_time {};
    Input m_lastInput {};
    int m_lastMouseSelection {};
    adt::time::Clock m_clockLastMouseSelection {};
    adt::time::Clock m_clockImageRedraw {};
    adt::time::Clock m_clockResize {};
    bool m_bNeedsResize {};
    bool m_bImageJustRedrawn {};
    adt::VStringM m_sTitle {};

    int m_aFdsWakeUp[2] {};

    /* */

public:
    virtual bool start(adt::Arena* pArena) final;
    virtual void destroy() final;
    virtual void draw() final;
    virtual void procEvents() final;
    virtual void seekFromInput() final;
    virtual void subStringSearch() final;
    virtual void wakeUp() final;

    /* */

protected:
    void adjustListHeight();
    void resizeHandler();
    int calcImageHeightSplit();

    void disableRawMode() noexcept(false); /* RuntimeException */
    void enableRawMode() noexcept(false); /* RuntimeException */

    void procInput();
    common::READ_STATUS readWChar();

    friend void sigwinchHandler(int sig);

    Input readFromStdin(const int timeoutMS);
    [[nodiscard]] ADT_NO_UB int parseSeq(adt::Span<char> spBuff, ssize_t nRead);
    [[nodiscard]] ADT_NO_UB MouseInput parseMouse(adt::Span<char> spBuff, ssize_t nRead);
    void procMouse(MouseInput in);

private:

#ifdef OPT_CHAFA
    void coverImage();
#endif
    void updateTitle();
    void tooSmall(int width, int height);
    void info();
    void volume();
    void time();
    void timeSlider();
    void songList();
    void scrollBar();
    void bottomLine();
    void errorMsg();
    void update();
    /* */
};

} /* namespace platform::ansi */

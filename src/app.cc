#include "app.hh"

#include "adt/logs.hh"
#include "platform/ansi/Win.hh"
#include "platform/termbox2/window.hh"

#ifdef USE_NCURSES
    #include "platform/ncurses/Win.hh"
#endif

#ifdef USE_NOTCURSES
    #include "platform/notcurses/Win.hh"
#endif

namespace app
{

UI_FRONTEND g_eUIFrontend = UI_FRONTEND::TERMBOX;
IWindow* g_pWin {};
bool g_bRunning {};
int g_argc {};
char** g_argv {};
VecBase<String> g_aArgs {};

Player* g_pPlayer {};
audio::IMixer* g_pMixer {};

IWindow*
allocWindow(IAllocator* pAlloc)
{
    IWindow* pRet {};

    switch (app::g_eUIFrontend)
    {
        case UI_FRONTEND::ANSI:
        {
            auto* pAnsiWin = (platform::ansi::Win*)alloc(pAlloc, 1, sizeof(platform::ansi::Win));
            *pAnsiWin = platform::ansi::Win();
            pRet = &pAnsiWin->super;
        }
        break;

        case UI_FRONTEND::TERMBOX:
        {
            auto* pTermboxWin = (platform::termbox2::Win*)alloc(pAlloc, 1, sizeof(platform::termbox2::Win));
            *pTermboxWin = platform::termbox2::Win();
            pRet = &pTermboxWin->super;
        }
        break;

        case UI_FRONTEND::NCURSES:
        {
#ifdef USE_NCURSES
            auto* pNCurses = (platform::ncurses::Win*)alloc(pAlloc, 1, sizeof(platform::ncurses::Win));
            *pNCurses = platform::ncurses::Win();
            pRet = &pNCurses->super;
#else
            CERR("UI_BACKEND::NCURSES: not available\n");
            exit(1);
#endif
        }
        break;

        case UI_FRONTEND::NOTCURSES:
        {
#ifdef USE_NOTCURSES
            auto* pNotCurses = (platform::notcurses::Win*)alloc(pAlloc, 1, sizeof(platform::notcurses::Win));
            *pNotCurses = platform::notcurses::Win();
            pRet = &pNotCurses->super;
#else
            CERR("UI_BACKEND::NCURSES: not available\n");
            exit(1);
#endif
        }
        break;
    }

    return pRet;
}

} /* namespace app */

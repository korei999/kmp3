#include "app.hh"

#include "adt/logs.hh"
#include "platform/termbox2/window.hh"

#ifdef USE_NCURSES
    #include "platform/ncurses/Win.hh"
#endif

namespace app
{

UI_BACKEND g_eUIBackend = UI_BACKEND::TERMBOX;
IWindow* g_pWin {};
bool g_bRunning {};
int g_argc {};
char** g_argv {};
VecBase<String> g_aArgs {};

Player* g_pPlayer {};
audio::IMixer* g_pMixer {};

IWindow*
allocWindow(IAllocator* pArena)
{
    IWindow* pRet {};

    switch (app::g_eUIBackend)
    {
        case UI_BACKEND::TERMBOX:
        {
            auto* pTermboxWin = (platform::termbox2::Win*)alloc(pArena, 1, sizeof(platform::termbox2::Win));
            *pTermboxWin = platform::termbox2::Win();
            pRet = &pTermboxWin->super;
        } break;

        case UI_BACKEND::NCURSES:
        {
#ifdef USE_NCURSES
            auto* pNCurses = (platform::ncurses::Win*)alloc(pArena, 1, sizeof(platform::ncurses::Win*));
            *pNCurses = platform::ncurses::Win();
            pRet = &pNCurses->super;
#else
            CERR("UI_BACKEND::NCURSES: not available\n");
            exit(1);
#endif
        } break;
    }

    return pRet;
}

} /* namespace app */

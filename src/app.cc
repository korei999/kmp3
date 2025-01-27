#include "app.hh"

#include "adt/logs.hh"
#include "platform/ansi/Win.hh"
#include "platform/termbox2/window.hh"

#ifdef USE_PIPEWIRE
    #include "platform/pipewire/Mixer.hh"
#endif

#ifdef USE_NCURSES
    #include "platform/ncurses/Win.hh"
#endif

#ifdef USE_NOTCURSES
    #include "platform/notcurses/Win.hh"
#endif

namespace app
{

UI g_eUIFrontend = UI::TERMBOX;
MIXER g_eMixer = MIXER::PIPEWIRE;
String g_sTerm {};
TERM g_eTerm = TERM::XTERM;
IWindow* g_pWin {};
bool g_bRunning {};
bool g_bNoImage {};
bool g_bSixelOrKitty {};
int g_argc {};
char** g_argv {};

Player* g_pPlayer {};
audio::IMixer* g_pMixer {};

IWindow*
allocWindow(IAllocator* pAlloc)
{
    IWindow* pRet {};

    switch (g_eUIFrontend)
    {
        default:
        {
            pRet = pAlloc->alloc<DummyWindow>();
        }
        break;

        case UI::ANSI:
        {
            pRet = pAlloc->alloc<platform::ansi::Win>();
        }
        break;

        case UI::TERMBOX:
        {
            pRet = pAlloc->alloc<platform::termbox2::Win>();
        }
        break;

        case UI::NCURSES:
        {
#ifdef USE_NCURSES
#else
            CERR("UI_BACKEND::NCURSES: not available\n");
            exit(1);
#endif
        }
        break;
    }

    return pRet;
}

audio::IMixer*
allocMixer(IAllocator* pAlloc)
{
    audio::IMixer* pMix {};

    switch (g_eMixer)
    {
        default:
        {
            pMix = pAlloc->alloc<audio::DummyMixer>();
        }
        break;

#ifdef USE_PIPEWIRE
        case MIXER::PIPEWIRE:
        {
            pMix = pAlloc->alloc<platform::pipewire::Mixer>();
        }
        break;
#endif
    }

    return pMix;
}

} /* namespace app */

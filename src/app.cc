#include "app.hh"

#include "adt/logs.hh"
#include "platform/ansi/Win.hh"
#include "platform/pipewire/Mixer.hh"
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
MIXER g_eMixer = MIXER::PIPEWIRE;
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

    switch (g_eUIFrontend)
    {
        case UI_FRONTEND::DUMMY:
        {
            DummyWindow* pDummy = (DummyWindow*)pAlloc->zalloc(1, sizeof(*pDummy));
            new(pDummy) DummyWindow();
            pRet = pDummy;
        }
        break;

        case UI_FRONTEND::ANSI:
        {
            auto* pAnsiWin = (platform::ansi::Win*)pAlloc->zalloc(1, sizeof(platform::ansi::Win));
            new(pAnsiWin) platform::ansi::Win();
            pRet = pAnsiWin;
        }
        break;

        case UI_FRONTEND::TERMBOX:
        {
            auto* pTermboxWin = (platform::termbox2::Win*)pAlloc->zalloc(1, sizeof(platform::termbox2::Win));
            new(pTermboxWin) platform::termbox2::Win();
            pRet = pTermboxWin;
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
    }

    return pRet;
}

audio::IMixer*
allocMixer(IAllocator* pAlloc)
{
    audio::IMixer* pMix {};

    switch (g_eMixer)
    {
        case MIXER::DUMMY:
        {
            audio::DummyMixer* pDummy = (decltype(pDummy))pAlloc->zalloc(1, sizeof(*pDummy));
            new(pDummy) audio::DummyMixer();
            pMix = pDummy;
        }
        break;

        case MIXER::PIPEWIRE:
        {
            platform::pipewire::Mixer* pPwMixer = (decltype(pPwMixer))pAlloc->zalloc(1, sizeof(*pPwMixer));
            new(pPwMixer) platform::pipewire::Mixer();
            pMix = pPwMixer;
        }
        break;
    }

    return pMix;
}

} /* namespace app */

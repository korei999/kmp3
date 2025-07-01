#include "app.hh"

#include "platform/ansi/Win.hh"

#ifdef OPT_PIPEWIRE
    #include "platform/pipewire/Mixer.hh"
#endif

#ifdef __APPLE__
    #include "platform/apple/apple.hh"
#endif

#ifdef OPT_SNDIO
    #include "platform/sndio/Mixer.hh"
#endif

using namespace adt;

namespace app
{

UI g_eUIFrontend {};
MIXER g_eMixer = MIXER::DUMMY;
StringView g_svTerm {};
TERM g_eTerm = TERM::XTERM;
IWindow* g_pWin {};
bool g_bRunning {};
bool g_bNoImage {};
bool g_bSixelOrKitty {};
bool g_bChafaSymbols {};

Config g_config {};
Player* g_pPlayer {};
audio::IMixer* g_pMixer {};
platform::ffmpeg::Decoder g_decoder {};

IWindow*
allocWindow(IAllocator* pAlloc)
{
    IWindow* pRet {};

    switch (g_eUIFrontend)
    {
        default:
        pRet = pAlloc->alloc<DummyWindow>();
        break;

        case UI::ANSI:
        pRet = pAlloc->alloc<platform::ansi::Win>();
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
        pMix = pAlloc->alloc<audio::DummyMixer>();
        break;

#ifdef OPT_PIPEWIRE
        case MIXER::PIPEWIRE:
        pMix = pAlloc->alloc<platform::pipewire::Mixer>();
        break;
#endif

#ifdef __APPLE__
        case MIXER::COREAUDIO:
        pMix = pAlloc->alloc<platform::apple::Mixer>();
        break;
#endif

#ifdef OPT_SNDIO
        case MIXER::SNDIO:
        pMix = pAlloc->alloc<platform::sndio::Mixer>();
        break;
#endif
    }

    return pMix;
}

} /* namespace app */

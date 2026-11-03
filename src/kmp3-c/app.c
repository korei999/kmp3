#include "app.h"

#include "klib/assert.h"

#include "defaults.h"

#ifdef OPT_PIPEWIRE
    #include "platform/pipewire/Mixer.h"
#endif

#ifdef OPT_ALSA
    #include "platform/alsa/Mixer.h"
#endif

bool app_g_bForceLoggerColors;

app_MIXER app_g_eMixer;
k_StringView app_g_svTerm;
app_TERM app_g_eTerm;
volatile bool app_g_vol_bRunning;
bool app_g_bNoImage;
bool app_g_bSixelOrKitty;
bool app_g_bChafaSymbols;

Window* app_g_pWin;
Config app_g_config = defaults_CONFIG;
Player* app_g_pPlayer;
audio_IMixer* app_g_pMixer;
ffmpeg_Decoder app_g_decoder;

audio_IMixer*
app_allocMixer(k_IAllocator* pAlloc)
{
    audio_IMixer* pMixer = NULL;

    switch (app_g_eMixer)
    {
        default: break;

        case app_MIXER_DUMMY:
        {
            audio_DummyMixer* pDummy = k_IAllocatorZalloc(pAlloc, sizeof(audio_DummyMixer));
            *pDummy = audio_DummyMixerCreate();
            pMixer = &pDummy->base;
        }
        break;

        case app_MIXER_ALSA:
        {
#ifdef OPT_ALSA
            platform_alsa_Mixer* pAlsa = k_IAllocatorZalloc(pAlloc, sizeof(*pAlsa));
            *pAlsa = platform_alsa_MixerCreate();
            pMixer = &pAlsa->base;
#endif
        }
        break;

        case app_MIXER_PIPEWIRE:
        {
#ifdef OPT_PIPEWIRE
            platform_pipewire_Mixer* pPw = k_IAllocatorZalloc(pAlloc, sizeof(*pPw));
            *pPw = platform_pipewire_MixerCreate();
            pMixer = &pPw->base;
#endif
        }
        break;

        case app_MIXER_COREAUDIO:
        {
            K_ASSERT(false, "implement this");
        }
        break;

        case app_MIXER_SNDIO:
        {
            K_ASSERT(false, "implement this");
        }
        break;
    }

    return pMixer;
}

#include "app.h"

#include "klib/assert.h"

#include "defaults.h"

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
// platform::ffmpeg::Decoder g_decoder {};

audio_IMixer*
app_allocMixer(k_IAllocator* pAlloc)
{
    audio_IMixer* pMixer = NULL;

    switch (app_g_eMixer)
    {
        case app_MIXER_DUMMY:
        {
            audio_DummyMixer* pDummy = k_IAllocatorZalloc(pAlloc, sizeof(audio_DummyMixer));
            audio_DummyMixerInit(pDummy);
            pMixer = &pDummy->base;
        }
        break;

        case app_MIXER_ALSA:
        {
            K_ASSERT(false, "implement this");
        }
        break;

        case app_MIXER_PIPEWIRE:
        {
            K_ASSERT(false, "implement this");
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

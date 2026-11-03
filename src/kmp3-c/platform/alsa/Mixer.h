#pragma once

#include "audio.h"

#include <alsa/asoundlib.h>

typedef struct platform_alsa_Mixer platform_alsa_Mixer;
struct platform_alsa_Mixer
{
    union
    {
        audio_IMixer;
        audio_IMixer base;
    };

    snd_pcm_t *pHandle;

    k_atomic_U8 atom_bRunning;
    k_atomic_U8 atom_bLoopDone;
    k_Thread thrdLoop;
    k_Mutex mtxLoop;
    k_CndVar cndLoop;

    uint32_t bufferTime; /* ring buffer length in us */
    uint32_t periodTime; /* period time in us */

    snd_pcm_sframes_t bufferSize;
    snd_pcm_sframes_t periodSize;

    snd_pcm_hw_params_t *pHwParams;
    snd_pcm_sw_params_t *pSwParams;
};

platform_alsa_Mixer platform_alsa_MixerCreate(void);

#pragma once

#include "audio.h"

#include <sndio.h>

typedef struct platform_sndio_Mixer platform_sndio_Mixer;
struct platform_sndio_Mixer
{
    union
    {
        audio_IMixer;
        audio_IMixer base;
    };

    struct sio_par par;
    struct sio_hdl* pHdl;

    k_atomic_U8 atom_bRunning;
    k_atomic_U8 atom_bLoopDone;
    k_Thread thrdLoop;
    k_Mutex mtxLoop;
    k_CndVar cndLoop;
};

platform_sndio_Mixer platform_sndio_MixerCreate(void);

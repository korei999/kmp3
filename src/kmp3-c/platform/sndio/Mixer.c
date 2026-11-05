#include "Mixer.h"

#include "app.h"

#include "klib/Ctx.h"

#ifdef OPT_MPRIS
    #include "platform/mpris/mpris.h"
#endif

#define PLATFORM_SNDIO_MIXER_N_BUF_FRAMES 1024

/* Virtual. */
static bool platform_sndio_MixerInit(platform_sndio_Mixer* s);
static void platform_sndio_MixerDeinit(platform_sndio_Mixer* s);
static bool platform_sndio_MixerPlay(platform_sndio_Mixer* s, k_StringView svPath);
static void platform_sndio_MixerPause(platform_sndio_Mixer* s, bool bPause);
static void platform_sndio_MixerChangeSampleRate(platform_sndio_Mixer* s, uint64_t sampleRate, bool bSave);
/* Virtual end. */

static void platform_sndio_MixerSetConfig(platform_sndio_Mixer* s, uint64_t sampleRate, int nChannels, bool bSaveNewConfig);
static K_THREAD_RESULT platform_sndio_MixerLoop(void* pArg);

platform_sndio_Mixer
platform_sndio_MixerCreate(void)
{
#define CAST_FUNC(func, name) (K_TYPEOF(((audio_IMixer*)(NULL))->vtable.func))platform_sndio_Mixer##name

    return (platform_sndio_Mixer){.vtable = {
        .init = CAST_FUNC(init, Init),
        .deinit = CAST_FUNC(deinit, Deinit),
        .play = CAST_FUNC(play, Play),
        .pause = CAST_FUNC(pause, Pause),
        .changeSampleRate = CAST_FUNC(changeSampleRate, ChangeSampleRate),
    }};

#undef CAST_FUNC
}

static bool
platform_sndio_MixerInit(platform_sndio_Mixer* s)
{
    K_CTX_LOG_DEBUG("initializing sndio");

    k_MutexInit(&s->mtxLoop, K_MUTEX_TYPE_PLAIN);
    k_CndVarInit(&s->cndLoop);

    s->bRunning = true;
    s->bMuted = false;

    const uint32_t sampleRate = 48000;
    const uint32_t nChannels = 2;
    const uint32_t bitsPerSample = 16;
    const uint32_t bytesPerSample = bitsPerSample / 8;

    s->pHdl = sio_open(SIO_DEVANY, SIO_PLAY, 0);
    if (!s->pHdl)
    {
        K_CTX_LOG_ERROR("sio_open(SIO_DEVANY, SIO_PLAY, 0)");
        return false;
    }

    sio_initpar(&s->par);
    s->par.bits = bitsPerSample;
    s->par.bps = bytesPerSample;
    s->par.sig = 1;
    s->par.le = 1;
    s->par.pchan = nChannels;
    s->par.rchan = 0;
    s->par.rate = sampleRate;
    s->par.appbufsz = PLATFORM_SNDIO_MIXER_N_BUF_FRAMES;

    s->par.xrun = SIO_IGNORE;

#define _TRY(FN, ...)                                                                                                  \
    if (!FN(__VA_ARGS__))                                                                                              \
    {                                                                                                                  \
        sio_close(s->pHdl);                                                                                            \
        K_CTX_LOG_ERROR(#FN);                                                                                          \
        return false;                                                                                                  \
    }

    k_atomic_U8StoreRelaxed(&s->atom_bRunning, true);

    _TRY(sio_setpar, s->pHdl, &s->par);
    _TRY(sio_start, s->pHdl);

#undef _TRY

    k_ThreadInit(&s->thrdLoop, platform_sndio_MixerLoop, s);

    return true;
}

static void
platform_sndio_MixerDeinit(platform_sndio_Mixer* s)
{
    k_atomic_U8StoreRelaxed(&s->atom_bRunning, false);
    platform_sndio_MixerPause(s, false); /* Sleeps in the loop() forever if paused. */

    while (!k_atomic_U8LoadRelaxed(&s->atom_bLoopDone))
        k_CndVarSignal(&s->cndLoop);

    k_ThreadJoin(&s->thrdLoop);
    k_MutexDestroy(&s->mtxLoop);
    k_CndVarDestroy(&s->cndLoop);

    sio_close(s->pHdl);

    K_CTX_LOG_DEBUG("sndio MixerDestroy()");
}

static bool
platform_sndio_MixerPlay(platform_sndio_Mixer* s, k_StringView svPath)
{
    const double prevSpeed = (double)s->changedSampleRate / (double)s->sampleRate;

    platform_sndio_MixerPause(s, true);
    if (!audio_IMixerPlayFinal(&s->base, svPath)) return false;

    platform_sndio_MixerSetConfig(s,
        ffmpeg_DecoderGetSampleRate(&app_g_decoder),
        ffmpeg_DecoderGetChannelsCount(&app_g_decoder),
        true
    );

    if (!k_doubleEq(prevSpeed, 1.0))
        platform_sndio_MixerChangeSampleRate(s, (double)s->sampleRate * prevSpeed, false);

    platform_sndio_MixerPause(s, false);

    return true;
}

static void
platform_sndio_MixerPause(platform_sndio_Mixer* s, bool bPause)
{
    bool bCurr = k_atomic_U8LoadRelaxed(&s->atom_bPaused);
    if (bCurr == bPause) return;

    k_atomic_U8StoreRelaxed(&s->atom_bPaused, bPause);

    k_MutexLock(&s->mtxLoop);

    if (bPause)
    {
        sio_stop(s->pHdl);
    }
    else
    {
        k_CndVarSignal(&s->cndLoop);
        sio_start(s->pHdl);
    }

    k_MutexUnlock(&s->mtxLoop);

#ifdef OPT_MPRIS
    platform_mpris_playbackStatusChanged();
#endif
}

static void
platform_sndio_MixerChangeSampleRate(platform_sndio_Mixer* s, uint64_t sampleRate, bool bSave)
{
    platform_sndio_MixerPause(s, true);
    platform_sndio_MixerSetConfig(s, sampleRate, s->nChannels, false);
    platform_sndio_MixerPause(s, false);

    if (bSave) s->sampleRate = sampleRate;
    s->changedSampleRate = sampleRate;
}

static void
platform_sndio_MixerSetConfig(platform_sndio_Mixer* s, uint64_t sampleRate, int nChannels, bool bSaveNewConfig)
{
    k_MutexLock(&s->mtxLoop);

    if (bSaveNewConfig)
    {
        s->changedSampleRate = s->sampleRate = sampleRate;
        s->nChannels = nChannels;
    }

    sampleRate = K_CLAMP(sampleRate, app_g_config.minSampleRate, app_g_config.maxSampleRate);
    nChannels = K_CLAMP(nChannels, 1, 20);

    s->par.pchan = nChannels;
    s->par.rate = sampleRate;

    sio_setpar(s->pHdl, &s->par);

    k_MutexUnlock(&s->mtxLoop);
}

static K_THREAD_RESULT
platform_sndio_MixerLoop(void* pArg)
{
    platform_sndio_Mixer* s = pArg;
    int writeStatus = 0;
    (void)writeStatus;

    /* sndio doesn't support float pcm. */
    int16_t* pRenderBuffer = calloc(1, K_ASIZE(audio_g_aDrainBuffer));

    while (k_atomic_U8LoadRelaxed(&s->atom_bRunning))
    {
        const float vol = s->bMuted ? 0.0f : powf(s->volume / 100.0f, 3.0f);

        const ssize_t nSamplesRequested = PLATFORM_SNDIO_MIXER_N_BUF_FRAMES * s->nChannels;
        audio_RingBufferPop(&s->ring, audio_g_aDrainBuffer, nSamplesRequested);

        for (ssize_t i = 0; i < nSamplesRequested; ++i)
        {
            const float sample = K_CLAMP(
                INT16_MAX * (audio_g_aDrainBuffer[i] * vol),
                (float)INT16_MIN,
                (float)INT16_MAX
            );
            pRenderBuffer[i] = (int16_t)sample;
        }

        {
            k_MutexLock(&s->mtxLoop);
            while (k_atomic_U8LoadRelaxed(&s->atom_bPaused))
            {
                k_CndVarWait(&s->cndLoop, &s->mtxLoop);

                if (!k_atomic_U8LoadRelaxed(&s->atom_bRunning))
                {
                    k_MutexUnlock(&s->mtxLoop);
                    goto done;
                }
            }

            writeStatus = sio_write(s->pHdl, pRenderBuffer, PLATFORM_SNDIO_MIXER_N_BUF_FRAMES * s->par.pchan * sizeof(int16_t));
            k_MutexUnlock(&s->mtxLoop);
        }
    }

done:
    k_atomic_U8StoreRelaxed(&s->atom_bLoopDone, true);
    free(pRenderBuffer);
    K_CTX_LOG_DEBUG("loop done");

    return (K_THREAD_RESULT){0};
}

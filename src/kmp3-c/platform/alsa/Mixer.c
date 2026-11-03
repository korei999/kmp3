#include "Mixer.h"

#include "app.h"

#include "klib/Ctx.h"

#ifdef OPT_MPRIS
    #include "platform/mpris/mpris.h"
#endif

static const char* PLATFORM_ALSA_DEVICE = "default";

/* Virtual. */
static bool platform_alsa_MixerInit(platform_alsa_Mixer* s);
static void platform_alsa_MixerDeinit(platform_alsa_Mixer* s);
static bool platform_alsa_MixerPlay(platform_alsa_Mixer* s, k_StringView svPath);
static void platform_alsa_MixerPause(platform_alsa_Mixer* s, bool bPause);
static void platform_alsa_MixerChangeSampleRate(platform_alsa_Mixer* s, uint64_t sampleRate, bool bSave);
/* Virtual end. */

static void platform_alsa_MixerSetConfig(platform_alsa_Mixer* s, uint64_t sampleRate, int nChannels, bool bSaveNewConfig);
static K_THREAD_RESULT platform_alsa_MixerLoop(void* pArg);

static int platform_alsa_MixerSetHwParams(platform_alsa_Mixer* s, snd_pcm_hw_params_t* params, snd_pcm_access_t access);
static int platform_alsa_MixerSetSwParams(platform_alsa_Mixer* s, snd_pcm_sw_params_t* swparams);

static void platform_alsa_MixerOpenAlsa(platform_alsa_Mixer* s);
static void platform_alsa_MixerXrunRecovery(platform_alsa_Mixer* s);

static void platform_alsa_errorHandler(const char* pFile, int line, const char* pFunc, int, const char* pFmt, ...);

platform_alsa_Mixer
platform_alsa_MixerCreate(void)
{
#define CAST_FUNC(func, name) (K_TYPEOF(((audio_IMixer*)(NULL))->vtable.func))platform_alsa_Mixer##name

    return (platform_alsa_Mixer){.vtable = {
        .init = CAST_FUNC(init, Init),
        .deinit = CAST_FUNC(deinit, Deinit),
        .play = CAST_FUNC(play, Play),
        .pause = CAST_FUNC(pause, Pause),
        .changeSampleRate = CAST_FUNC(changeSampleRate, ChangeSampleRate),
    }};

#undef CAST_FUNC
}

bool
platform_alsa_MixerInit(platform_alsa_Mixer* s)
{
    k_CndVarInit(&s->cndLoop);
    k_MutexInit(&s->mtxLoop, K_MUTEX_TYPE_PLAIN);
    s->bufferTime = 500000;
    s->periodTime = 100000;

    K_CTX_LOG_DEBUG("initializing alsa");

    platform_alsa_MixerOpenAlsa(s);
    k_atomic_U8StoreRelaxed(&s->atom_bRunning, true);
    k_ThreadInit(&s->thrdLoop, platform_alsa_MixerLoop, s);

    return true;
}

void
platform_alsa_MixerDeinit(platform_alsa_Mixer* s)
{
    k_atomic_U8StoreRelaxed(&s->atom_bRunning, false);
    platform_alsa_MixerPause(s, false); /* Sleeps in the loop() forever if paused. */

    while (k_atomic_U8LoadRelaxed(&s->atom_bLoopDone) != true)
        k_CndVarSignal(&s->cndLoop);

    k_ThreadJoin(&s->thrdLoop);
    k_MutexDestroy(&s->mtxLoop);
    k_CndVarDestroy(&s->cndLoop);

    // snd_pcm_drain(s->pHandle); /* Too slow. */
    snd_pcm_close(s->pHandle);

    snd_pcm_hw_params_free(s->pHwParams);
    snd_pcm_sw_params_free(s->pSwParams);

    K_CTX_LOG_DEBUG("alsa mixer deinit");
}

bool
platform_alsa_MixerPlay(platform_alsa_Mixer* s, k_StringView svPath)
{
    const double prevSpeed = (double)s->changedSampleRate / (double)s->sampleRate;

    platform_alsa_MixerPause(s, true);

    if (!audio_IMixerPlayFinal(&s->base, svPath)) return false;

    platform_alsa_MixerSetConfig(s, ffmpeg_DecoderGetSampleRate(&app_g_decoder), ffmpeg_DecoderGetChannelsCount(&app_g_decoder), true);

    if (!k_doubleEq(prevSpeed, 1.0))
        platform_alsa_MixerChangeSampleRate(s, (double)s->sampleRate * prevSpeed, false);

    platform_alsa_MixerPause(s, false);

    return true;
}

void
platform_alsa_MixerPause(platform_alsa_Mixer* s, bool bPause)
{
    bool bCurr = k_atomic_U8LoadRelaxed(&s->atom_bPaused);
    if (bCurr == bPause) return;

    K_CTX_LOG_INFO("bPause: {b}\n", bPause);
    k_atomic_U8StoreRelaxed(&s->atom_bPaused, bPause);

    k_MutexLock(&s->mtxLoop);

    if (bPause)
    {
    }
    else
    {
        k_CndVarSignal(&s->cndLoop);
    }

    k_MutexUnlock(&s->mtxLoop);

#ifdef OPT_MPRIS
    platform_mpris_playbackStatusChanged();
#endif
}

void
platform_alsa_MixerChangeSampleRate(platform_alsa_Mixer* s, uint64_t sampleRate, bool bSave)
{
    int err = 0;

    platform_alsa_MixerPause(s, true);

    err = snd_pcm_set_params(
        s->pHandle,
        SND_PCM_FORMAT_FLOAT,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        s->nChannels, sampleRate, 1, 50000
    );

    platform_alsa_MixerPause(s, false);

    if (err < 0)
    {
        K_CTX_LOG_ERROR("{nts}", snd_strerror(err));
        return;
    }

    if (bSave) s->sampleRate = sampleRate;
    s->changedSampleRate = sampleRate;
}

void
platform_alsa_MixerSetConfig(platform_alsa_Mixer* s, uint64_t sampleRate, int nChannels, bool bSaveNewConfig)
{
    k_MutexLock(&s->mtxLoop);

    int err = 0;

    if (bSaveNewConfig)
    {
        s->changedSampleRate = s->sampleRate = sampleRate;
        s->nChannels = nChannels;
    }

    sampleRate = K_CLAMP(sampleRate, app_g_config.minSampleRate, app_g_config.maxSampleRate);
    nChannels = K_CLAMP(nChannels, 1, 20);

    err = snd_pcm_set_params(
        s->pHandle,
        SND_PCM_FORMAT_FLOAT,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        nChannels, sampleRate, 1, 50000
    );

    if (err < 0)
    {
        K_CTX_LOG_ERROR("({i}): {nts}", err, snd_strerror(err));
        /* FIXME: Recoverable? */
    }

    k_MutexUnlock(&s->mtxLoop);
}

K_THREAD_RESULT
platform_alsa_MixerLoop(void* pArg)
{
    platform_alsa_Mixer* s = pArg;

    k_CtxInitArenaForThisThread(K_SIZE_1K);
    snd_pcm_start(s->pHandle);

    bool bPaused = true;
    bool bReinit = false;

    while (k_atomic_U8LoadRelaxed(&s->atom_bRunning))
    {
        ssize_t nSamplesRequested = 0;
        const float vol = s->bMuted ? 0.0f : powf((float)s->volume / 100.0f, 3.0f);

        nSamplesRequested = s->periodSize * s->nChannels;
        audio_RingBufferPop(&s->ring, audio_g_aDrainBuffer, nSamplesRequested);

        for (ssize_t sampleI = 0; sampleI < nSamplesRequested; ++sampleI)
            audio_g_aDrainBuffer[sampleI] *= vol;

        k_MutexLock(&s->mtxLoop);
        {
            while (k_atomic_U8LoadRelaxed(&s->atom_bPaused))
            {
                bPaused = true;
                snd_pcm_drop(s->pHandle);
                k_CndVarWait(&s->cndLoop, &s->mtxLoop);

                if (!k_atomic_U8LoadRelaxed(&s->atom_bRunning))
                {
                    k_MutexUnlock(&s->mtxLoop);
                    goto done;
                }
            }

            if (bPaused)
            {
                bPaused = false;
                bReinit = false;
                snd_pcm_prepare(s->pHandle);
            }

            if (bReinit)
            {
                bReinit = false;
                platform_alsa_MixerXrunRecovery(s);
            }

            float* ptr = audio_g_aDrainBuffer;
            ssize_t cptr = s->periodSize;
            while (cptr > 0)
            {
                snd_pcm_sframes_t err = snd_pcm_writei(s->pHandle, ptr, cptr);
                if (err == -EAGAIN) continue;

                if (err < 0)
                {
                    bReinit = true;
                    break;
                }

                ptr += err * s->nChannels;
                cptr -= err;
            }
        }
        k_MutexUnlock(&s->mtxLoop);
    }

done:
    k_atomic_U8StoreRelaxed(&s->atom_bLoopDone, true);
    K_CTX_LOG_DEBUG("alsa loop done");
    k_CtxDestroyArenaForThisThread();

    return (K_THREAD_RESULT){0};
}

int
platform_alsa_MixerSetHwParams(platform_alsa_Mixer* s, snd_pcm_hw_params_t* params, snd_pcm_access_t access)
{
    unsigned int rrate;
    snd_pcm_uframes_t size;
    int err, dir;

    /* choose all parameters */
    err = snd_pcm_hw_params_any(s->pHandle, params);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Broken configuration for playback: no configurations available: {nts}", snd_strerror(err));
        return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(s->pHandle, params, 1);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Resampling setup failed for playback: {nts}", snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(s->pHandle, params, access);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Access type not available for playback: {nts}", snd_strerror(err));
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(s->pHandle, params, SND_PCM_FORMAT_FLOAT);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Sample format not available for playback: {nts}", snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(s->pHandle, params, s->nChannels);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Channels count ({i}) not available for playbacks: {nts}", s->nChannels, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = s->sampleRate;
    err = snd_pcm_hw_params_set_rate_near(s->pHandle, params, &rrate, 0);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Rate {u32}Hz not available for playback: {nts}", s->sampleRate, snd_strerror(err));
        return err;
    }
    if (rrate != s->sampleRate)
    {
        K_CTX_LOG_ERROR("Rate doesn't match (requested {u32}Hz, get {i}Hz)", s->sampleRate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(s->pHandle, params, &s->bufferTime, &dir);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Unable to set buffer time {u32} for playback: {nts}", s->bufferTime, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &size);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Unable to get buffer size for playback: {nts}", snd_strerror(err));
        return err;
    }
    s->bufferSize = size;
    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(s->pHandle, params, &s->periodTime, &dir);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Unable to set period time {u32} for playback: {nts}", s->periodTime, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Unable to get period size for playback: {nts}", snd_strerror(err));
        return err;
    }
    s->periodSize = size;
    K_CTX_LOG_DEBUG("pediodSize: {i64}", s->periodSize);
    /* write the parameters to device */
    err = snd_pcm_hw_params(s->pHandle, params);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Unable to set hw params for playback: {nts}", snd_strerror(err));
        return err;
    }
    return 0;
}

int
platform_alsa_MixerSetSwParams(platform_alsa_Mixer* s, snd_pcm_sw_params_t* swparams)
{
    int err;
    int periodEvent = 0;                /* produce poll event after each period */

    /* get the current swparams */
    err = snd_pcm_sw_params_current(s->pHandle, swparams);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Unable to determine current swparams for playback: {nts}", snd_strerror(err));
        return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(s->pHandle, swparams, (s->bufferSize / s->periodSize) * s->periodSize);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Unable to set start threshold mode for playback: {nts}", snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    err = snd_pcm_sw_params_set_avail_min(s->pHandle, swparams, periodEvent ? s->bufferSize : s->periodSize);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Unable to set avail min for playback: {nts}", snd_strerror(err));
        return err;
    }
    /* enable period events when requested */
    if (periodEvent)
    {
        err = snd_pcm_sw_params_set_period_event(s->pHandle, swparams, 1);
        if (err < 0)
        {
            K_CTX_LOG_ERROR("Unable to set period event: {nts}", snd_strerror(err));
            return err;
        }
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(s->pHandle, swparams);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("Unable to set sw params for playback: {nts}", snd_strerror(err));
        return err;
    }
    return 0;
}

void
platform_alsa_MixerOpenAlsa(platform_alsa_Mixer* s)
{
    int err = 0;

    err = snd_pcm_open(&s->pHandle, PLATFORM_ALSA_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("({i}): {nts}", err, snd_strerror(err));
        /* FIXME: quit? */
    }

    snd_pcm_hw_params_malloc(&s->pHwParams);
    snd_pcm_sw_params_malloc(&s->pSwParams);

    snd_lib_error_set_handler(platform_alsa_errorHandler);

    err = platform_alsa_MixerSetHwParams(s, s->pHwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("({i}): {nts}", err, snd_strerror(err));
    }

    err = platform_alsa_MixerSetSwParams(s, s->pSwParams);
    if (err < 0)
    {
        K_CTX_LOG_ERROR("({i}): {nts}", err, snd_strerror(err));
    }
}

void
platform_alsa_MixerXrunRecovery(platform_alsa_Mixer* s)
{
    K_CTX_LOG_WARN("reinitializing alsa...");
    snd_pcm_drop(s->pHandle);
    snd_pcm_close(s->pHandle);
    snd_pcm_hw_params_free(s->pHwParams);
    snd_pcm_sw_params_free(s->pSwParams);

    platform_alsa_MixerOpenAlsa(s);
}

static void
platform_alsa_errorHandler(const char* pFile, int line, const char* pFunc, int _, const char* pFmt, ...)
{
    (void)_;

    va_list args;
    char aBuff[256];
    va_start(args, pFmt);
    ssize_t n = vsnprintf(aBuff, sizeof(aBuff), pFmt, args);
    va_end(args);

    K_CTX_LOG_ERROR("({nts}, {i}, {nts}): {PSv}\n", pFile, line, pFunc, &(k_StringView){aBuff, n});
}

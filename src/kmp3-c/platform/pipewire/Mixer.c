#include "Mixer.h"

#include "app.h"

#include "klib/Ctx.h"
#include "klib/assert.h"

#define PW(name) platform_pipewire_##name

static uint32_t PW(formatByteSize)(enum spa_audio_format eFormat);

/* vtable */
static bool PW(MixerInit)(PW(Mixer)* s);
static void PW(MixerDeinit)(PW(Mixer)* s);
static bool PW(MixerPlay)(PW(Mixer)* s, k_StringView svPath);
static void PW(MixerPause)(PW(Mixer)* s, bool bPause);
static void PW(MixerChangeSampleRate)(PW(Mixer)* s, uint64_t sampleRate, bool bSave);
static void PW(MixerSetNChannels)(PW(Mixer)* s, uint32_t nChannels);
static void PW(MixerOnProcessCB)(void* pArg);
/* vtable end */

static const struct pw_stream_events PW(s_streamEvents) = {
    .version = PW_VERSION_STREAM_EVENTS,
    .process = PW(MixerOnProcessCB),
};

K_UNUSED static uint32_t
PW(formatByteSize)(enum spa_audio_format eFormat)
{
    switch (eFormat)
    {
        default: return 1;

        case SPA_AUDIO_FORMAT_S16_LE:
        case SPA_AUDIO_FORMAT_S16_BE:
        case SPA_AUDIO_FORMAT_U16_LE:
        case SPA_AUDIO_FORMAT_U16_BE:
        return 2;

        case SPA_AUDIO_FORMAT_S24_LE:
        case SPA_AUDIO_FORMAT_S24_BE:
        case SPA_AUDIO_FORMAT_U24_LE:
        case SPA_AUDIO_FORMAT_U24_BE:
        return 3;

        case SPA_AUDIO_FORMAT_S24_32_LE:
        case SPA_AUDIO_FORMAT_S24_32_BE:
        case SPA_AUDIO_FORMAT_U24_32_LE:
        case SPA_AUDIO_FORMAT_U24_32_BE:
        case SPA_AUDIO_FORMAT_S32_LE:
        case SPA_AUDIO_FORMAT_S32_BE:
        case SPA_AUDIO_FORMAT_U32_LE:
        case SPA_AUDIO_FORMAT_U32_BE:
        case SPA_AUDIO_FORMAT_F32_LE:
        case SPA_AUDIO_FORMAT_F32_BE:
        return 4;

        case SPA_AUDIO_FORMAT_F64_LE:
        case SPA_AUDIO_FORMAT_F64_BE:
        return 8;
    };
}

PW(Mixer)
PW(MixerCreate)(void)
{
#define CAST_FUNC(func, name) (K_TYPEOF(((audio_IMixer*)(NULL))->vtable.func))PW(Mixer##name)

    return (PW(Mixer)){.vtable = {
        .init = CAST_FUNC(init, Init),
        .deinit = CAST_FUNC(deinit, Deinit),
        .play = CAST_FUNC(play, Play),
        .pause = CAST_FUNC(pause, Pause),
        .changeSampleRate = CAST_FUNC(changeSampleRate, ChangeSampleRate),
    }};

#undef CAST_FUNC
}

static bool
PW(MixerInit)(PW(Mixer)* s)
{
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    s->bRunning = true;

    s->sampleRate = 48000;
    s->nChannels = 2;
    s->eformat = SPA_AUDIO_FORMAT_F32;

    pw_init(NULL, NULL);

    const ssize_t setupBuffSize = 1024;
    uint8_t* pSetupBuff = k_ArenaZalloc(pArena, setupBuffSize);
    const struct spa_pod* aParams[1] = {0};
    struct spa_pod_builder b = {0};
    spa_pod_builder_init(&b, pSetupBuff, setupBuffSize);

    s->pThrdLoop = pw_thread_loop_new("kmp3PwThreadLoop", NULL);

    s->pStream = pw_stream_new_simple(
        pw_thread_loop_get_loop(s->pThrdLoop),
        "kmp3AudioSource",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "Music",
            NULL
        ),
        &PW(s_streamEvents),
        s
    );

    K_ASSERT_ALWAYS(s->pStream != NULL, "");

    struct spa_audio_info_raw rawInfo = {
        .format = s->eformat,
        .rate = s->sampleRate,
        .channels = s->nChannels,
    };

    aParams[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    pw_stream_connect(
        s->pStream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE | PW_STREAM_FLAG_MAP_BUFFERS,
        aParams,
        K_ASIZE(aParams)
    );
    pw_thread_loop_start(s->pThrdLoop);

    K_CTX_LOG_DEBUG("pipewire initialized");
    k_ArenaStateRestore(&arenaState);
    return true;
}

static void
PW(MixerDeinit)(PW(Mixer)* s)
{
    pw_thread_loop_lock(s->pThrdLoop);
    pw_stream_set_active(s->pStream, true);
    pw_thread_loop_unlock(s->pThrdLoop);

    pw_thread_loop_stop(s->pThrdLoop);
    K_CTX_LOG_DEBUG("pw_thread_loop_stop()");

    if (s->pStream) pw_stream_destroy(s->pStream);
    if (s->pThrdLoop) pw_thread_loop_destroy(s->pThrdLoop);
    pw_deinit();

    K_CTX_LOG_DEBUG("MixerDestroy()");
}

static bool
PW(MixerPlay)(PW(Mixer)* s, k_StringView svPath)
{
    const double prevSpeed = (double)s->changedSampleRate / (double)s->sampleRate;

    PW(MixerPause)(s, true);

    if (!audio_IMixerPlayFinal(&s->base, svPath)) return false;

    PW(MixerSetNChannels)(s, ffmpeg_DecoderGetChannelsCount(&app_g_decoder));
    PW(MixerChangeSampleRate)(s, ffmpeg_DecoderGetSampleRate(&app_g_decoder), true);

    if (!k_doubleEq(prevSpeed, 1.0))
        PW(MixerChangeSampleRate)(s, (double)s->sampleRate * prevSpeed, false);

    PW(MixerPause)(s, false);

    return true;
}

static void
PW(MixerPause)(PW(Mixer)* s, bool bPause)
{
    k_atomic_U8StoreRelaxed(&s->atom_bPaused, bPause);

    pw_thread_loop_lock(s->pThrdLoop);
    pw_stream_set_active(s->pStream, !bPause);

    K_CTX_LOG_INFO("bPaused: {b}", s->atom_bPaused.volNum);
    pw_thread_loop_unlock(s->pThrdLoop);

    // mpris::playbackStatusChanged();
}

static void
PW(MixerChangeSampleRate)(PW(Mixer)* s, uint64_t sampleRate, bool bSave)
{
    sampleRate = K_CLAMP(sampleRate, app_g_config.minSampleRate, app_g_config.maxSampleRate);

    uint8_t aSetupBuff[512] = {0};
    struct spa_audio_info_raw rawInfo = {
        .format = s->eformat,
        .rate = sampleRate,
        .channels = s->nChannels,
    };

    struct spa_pod_builder b = {0};
    spa_pod_builder_init(&b, aSetupBuff, sizeof(aSetupBuff));

    const struct spa_pod* aParams[1] = {0};
    aParams[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    pw_thread_loop_lock(s->pThrdLoop);
    {
        pw_stream_update_params(s->pStream, aParams, K_ASIZE(aParams));

        /* update won't apply without this */
        {
            bool bPaused = k_atomic_U8LoadRelaxed(&s->atom_bPaused);
            pw_stream_set_active(s->pStream, bPaused);
            pw_stream_set_active(s->pStream, !bPaused);
        }

        if (bSave) s->sampleRate = sampleRate;
        s->changedSampleRate = sampleRate;
    }
    pw_thread_loop_unlock(s->pThrdLoop);
}

static void
PW(MixerSetNChannels)(PW(Mixer)* s, uint32_t nChannels)
{
    s->nChannels = nChannels;

    uint8_t aSetupBuff[512] = {0};
    struct spa_audio_info_raw rawInfo = {
        .format = s->eformat,
        .rate = s->sampleRate,
        .channels = nChannels,
    };

    struct spa_pod_builder b = {0};
    spa_pod_builder_init(&b, aSetupBuff, sizeof(aSetupBuff));

    const struct spa_pod* pParams = NULL;
    pParams = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    pw_thread_loop_lock(s->pThrdLoop);
    {
        pw_stream_update_params(s->pStream, &pParams, 1);

        /* won't apply without this */
        {
            bool bPaused = k_atomic_U8LoadRelaxed(&s->atom_bPaused);
            pw_stream_set_active(s->pStream, bPaused);
            pw_stream_set_active(s->pStream, !bPaused);
        }
    }
    pw_thread_loop_unlock(s->pThrdLoop);
}

static void
PW(MixerOnProcessCB)(void* pArg)
{
    PW(Mixer)* s = pArg;

    if (!k_atomic_U8LoadRelaxed(&s->atom_bArena))
    {
        k_CtxInitArenaForThisThread(K_SIZE_1K * 8);
        k_atomic_U8StoreRelaxed(&s->atom_bArena, true);
    }

    struct pw_buffer* pPwBuffer = pw_stream_dequeue_buffer(s->pStream);
    if (!pPwBuffer)
    {
        K_CTX_LOG_DEBUG("out of buffers");
        return;
    }

    struct spa_data pBuffData = pPwBuffer->buffer->datas[0];
    float* pDest = (float*)pBuffData.data;

    if (!pDest)
    {
        K_CTX_LOG_DEBUG("pDest == NULL");
        return;
    }

    uint32_t stride = PW(formatByteSize)(s->eformat) * s->nChannels;
    uint32_t nFramesRequested = (stride > 0) ? (pBuffData.maxsize / stride) : 0;
    if (pPwBuffer->requested) nFramesRequested = K_MIN((uint64_t)pPwBuffer->requested, (uint64_t)nFramesRequested);

    if (nFramesRequested*s->nChannels > K_ASIZE(audio_g_aDrainBuffer)) nFramesRequested = K_ASIZE(audio_g_aDrainBuffer);

    const float vol = s->bMuted ? 0.0f : pow(s->volume * (1.0f/100.0f), 3.0f);
    const ssize_t nSamplesRequested = nFramesRequested * s->nChannels;
    audio_RingBufferPop(&s->ring, pDest, nSamplesRequested);

    for (ssize_t sampleI = 0; sampleI < nSamplesRequested; ++sampleI)
        pDest[sampleI] *= vol;

    pBuffData.chunk->offset = 0;
    pBuffData.chunk->stride = stride;
    pBuffData.chunk->size = nFramesRequested * stride;

    pw_stream_queue_buffer(s->pStream, pPwBuffer);
}

#undef PW

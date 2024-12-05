#include "Mixer.hh"

#include "adt/guard.hh"
#include "adt/logs.hh"
#include "adt/utils.hh"
#include "adt/math.hh"
#include "app.hh"
#include "mpris.hh"

#include <cmath>

#include <emmintrin.h>
#include <immintrin.h>

namespace platform
{
namespace pipewire
{

struct ThrdLoopLockGuard
{
    pw_thread_loop* p;

    ThrdLoopLockGuard() = delete;
    ThrdLoopLockGuard(pw_thread_loop* _p) : p(_p) { pw_thread_loop_lock(_p); }
    ~ThrdLoopLockGuard() { pw_thread_loop_unlock(p); }
};

static void MixerRunThread(Mixer* s, int argc, char** argv);
static void onProcess(void* data);

static const pw_stream_events s_streamEvents {
    .version = PW_VERSION_STREAM_EVENTS,
    .destroy {},
    .state_changed {},
    .control_info {},
    .io_changed {},
    .param_changed {},
    .add_buffer {},
    .remove_buffer {},
    .process = onProcess,
    .drained {},
    .command {},
    .trigger_done {},
};

static f32 s_aPwBuff[audio::CHUNK_SIZE] {}; /* big */

static u32
formatByteSize(enum spa_audio_format eFormat)
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

void
MixerInit(Mixer* s)
{
    s->base.bRunning = true;
    s->base.bMuted = false;
    s->base.volume = 0.1f;

    s->base.sampleRate = 48000;
    s->nChannels = 2;
    s->eformat = SPA_AUDIO_FORMAT_F32;

    mtx_init(&s->mtxDecoder, mtx_plain);
    mtx_init(&s->mtxThrdLoop, mtx_plain);
    cnd_init(&s->cndThrdLoop);

    MixerRunThread(s, app::g_argc, app::g_argv);
}

void
MixerDestroy(Mixer* s)
{
    {
        ThrdLoopLockGuard tLock(s->pThrdLoop);
        pw_stream_set_active(s->pStream, true);
    }

    pw_thread_loop_stop(s->pThrdLoop);
    LOG_NOTIFY("pw_thread_loop_stop()\n");

    s->base.bRunning = false;

    if (s->bDecodes) ffmpeg::DecoderClose(s->pDecoder);

    pw_stream_destroy(s->pStream);
    pw_thread_loop_destroy(s->pThrdLoop);
    pw_deinit();

    mtx_destroy(&s->mtxDecoder);
    mtx_destroy(&s->mtxThrdLoop);
    cnd_destroy(&s->cndThrdLoop);
    LOG_NOTIFY("MixerDestroy()\n");
}

void
MixerPlay(Mixer* s, String sPath)
{
    const f64 prevSpeed = f64(s->base.changedSampleRate) / f64(s->base.sampleRate);

    MixerPause(s, true);

    {
        guard::Mtx lockDec(&s->mtxDecoder);

        s->sPath = sPath;

        if (s->bDecodes) ffmpeg::DecoderClose(s->pDecoder);

        auto err = ffmpeg::DecoderOpen(s->pDecoder, sPath);
        if (err != ffmpeg::ERROR::OK)
        {
            LOG_WARN("DecoderOpen\n");
            return;
        }
        s->bDecodes = true;
    }

    s->base.nChannels = ffmpeg::DecoderGetChannelsCount(s->pDecoder);
    MixerChangeSampleRate(s, ffmpeg::DecoderGetSampleRate(s->pDecoder), true);

    if (!math::eq(prevSpeed, 1.0))
        audio::MixerChangeSampleRate(s, f64(s->base.sampleRate) * prevSpeed, false);

    MixerPause(s, false);

    s->base.bUpdateMpris = true; /* mark to update in frame::run() */ 
}

static void
MixerRunThread(Mixer* s, int argc, char** argv)
{
    pw_init(&argc, &argv);

    u8 setupBuffer[1024] {};
    const spa_pod* aParams[1] {};
    spa_pod_builder b {};
    spa_pod_builder_init(&b, setupBuffer, sizeof(setupBuffer));

    s->pThrdLoop = pw_thread_loop_new("kmp3PwThreadLoop", {});

    s->pStream = pw_stream_new_simple(
        pw_thread_loop_get_loop(s->pThrdLoop),
        "kmp3AudioSource",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "Music",
            nullptr
        ),
        &s_streamEvents,
        s
    );

    spa_audio_info_raw rawInfo {
        .format = s->eformat,
        .flags {},
        .rate = s->base.sampleRate,
        .channels = s->nChannels,
        .position {}
    };

    aParams[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    pw_stream_connect(
        s->pStream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT|PW_STREAM_FLAG_INACTIVE|PW_STREAM_FLAG_MAP_BUFFERS),
        aParams,
        utils::size(aParams)
    );

    pw_thread_loop_start(s->pThrdLoop);
}

static void
writeFramesLocked(Mixer* s, f32* pBuff, u32 nFrames, long* pSamplesWritten, u64* pPcmPos)
{
    ffmpeg::ERROR err {};
    {
        guard::Mtx lock(&s->mtxDecoder);

        if (!s->bDecodes) return;

        err = ffmpeg::DecoderWriteToBuffer(s->pDecoder, pBuff, utils::size(s_aPwBuff), nFrames, s->nChannels, pSamplesWritten, pPcmPos);
        if (err == ffmpeg::ERROR::END_OF_FILE)
        {
            MixerPause(s, true);
            ffmpeg::DecoderClose(s->pDecoder);
            s->bDecodes = false;

        }
    }

    if (err == ffmpeg::ERROR::END_OF_FILE) PlayerOnSongEnd(app::g_pPlayer);
}

static void
onProcess(void* pData)
{
    auto* s = (Mixer*)pData;

    pw_buffer* pPwBuffer = pw_stream_dequeue_buffer(s->pStream);
    if (!pPwBuffer)
    {
        pw_log_warn("out of buffers: %m");
        return;
    }

    auto pBuffData = pPwBuffer->buffer->datas[0];
    auto* pDest = (f32*)pBuffData.data;

    if (!pDest)
    {
        LOG_WARN("pDest == nullptr\n");
        return;
    }

    u32 stride = formatByteSize(s->eformat) * s->nChannels;
    u32 nFrames = pBuffData.maxsize / stride;
    if (pPwBuffer->requested) nFrames = SPA_MIN(pPwBuffer->requested, (u64)nFrames);

    if (nFrames * s->nChannels > utils::size(s_aPwBuff)) nFrames = utils::size(s_aPwBuff);

    s->nLastFrames = nFrames;

    static long nDecodedSamples = 0;
    static long nWrites = 0;

    f32 vol = s->base.bMuted ? 0.0f : std::pow(s->base.volume, 3);

    for (u32 frameIdx = 0; frameIdx < nFrames; frameIdx++)
    {
        for (u32 chIdx = 0; chIdx < s->nChannels; chIdx++)
        {
            /* modify each sample here */
            *pDest++ = s_aPwBuff[nWrites++] * vol;
        }

        if (nWrites >= nDecodedSamples)
        {
            /* ask to fill the buffer when it's empty */
            writeFramesLocked(s, s_aPwBuff, nFrames, &nDecodedSamples, &s->base.currentTimeStamp);
            nWrites = 0;
        }
    }

    if (nDecodedSamples == 0)
    {
        s->base.currentTimeStamp = 0;
        s->base.totalSamplesCount = 0;
    }
    else
    {
        s->base.totalSamplesCount = ffmpeg::DecoderGetTotalSamplesCount(s->pDecoder);
    }

    pBuffData.chunk->offset = 0;
    pBuffData.chunk->stride = stride;
    pBuffData.chunk->size = nFrames * stride;

    pw_stream_queue_buffer(s->pStream, pPwBuffer);
}

void
MixerPause(Mixer* s, bool bPause)
{
    ThrdLoopLockGuard lock(s->pThrdLoop);
    pw_stream_set_active(s->pStream, !bPause);
    s->base.bPaused = bPause;

    LOG_NOTIFY("bPaused: {}\n", s->base.bPaused);
    mpris::playbackStatusChanged();
}

void
MixerTogglePause(Mixer* s)
{
    MixerPause(s, !s->base.bPaused);
}

void
MixerChangeSampleRate(Mixer* s, u64 sampleRate, bool bSave)
{
    u8 aSetupBuff[512] {};
    spa_audio_info_raw rawInfo {
        .format = s->eformat,
        .flags {},
        .rate = static_cast<u32>(sampleRate),
        .channels = s->nChannels,
        .position {}
    };

    spa_pod_builder b {};
    spa_pod_builder_init(&b, aSetupBuff, sizeof(aSetupBuff));

    const spa_pod* aParams[1] {};
    aParams[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    ThrdLoopLockGuard lock(s->pThrdLoop);
    pw_stream_update_params(s->pStream, aParams, utils::size(aParams));
    /* update won't apply without this */
    pw_stream_set_active(s->pStream, s->base.bPaused);
    pw_stream_set_active(s->pStream, !s->base.bPaused);

    if (bSave) s->base.sampleRate = sampleRate;

    s->base.changedSampleRate = sampleRate;
}

void
MixerSeekMS(Mixer* s, long ms)
{
    guard::Mtx lock(&s->mtxDecoder);
    if (!s->bDecodes) return;

    long maxMs = audio::MixerGetMaxMS(s);
    ms = utils::clamp(ms, 0L, maxMs);
    ffmpeg::DecoderSeekMS(s->pDecoder, ms);

    s->base.currentTimeStamp = f64(ms)/1000.0 * s->base.sampleRate * s->base.nChannels;
    s->base.totalSamplesCount = ffmpeg::DecoderGetTotalSamplesCount(s->pDecoder);

    mpris::seeked();
}

void
MixerSeekLeftMS(Mixer* s, long ms)
{
    long currMs = audio::MixerGetCurrentMS(s);
    MixerSeekMS(s, currMs - ms);
}

void
MixerSeekRightMS(Mixer* s, long ms)
{
    long currMs = audio::MixerGetCurrentMS(s);
    MixerSeekMS(s, currMs + ms);
}

Opt<String>
MixerGetMetadata(Mixer* s, const String sKey)
{
    return ffmpeg::DecoderGetMetadataValue(s->pDecoder, sKey);
}

} /* namespace pipewire */
} /* namespace platform */

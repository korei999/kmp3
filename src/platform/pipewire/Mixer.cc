#include "Mixer.hh"

#include "logs.hh"
#include "adt/utils.hh"
#include "app.hh"
#include "adt/guard.hh"

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
static bool MixerEmpty(Mixer* s);

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
static u32 s_pwBuffSize = 0;

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

    s->sampleRate = 48000;
    s->channels = 2;
    s->eformat = SPA_AUDIO_FORMAT_F32;

    mtx_init(&s->mtxAdd, mtx_plain);
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

    mtx_destroy(&s->mtxAdd);
    mtx_destroy(&s->mtxDecoder);
    mtx_destroy(&s->mtxThrdLoop);
    cnd_destroy(&s->cndThrdLoop);
    LOG_NOTIFY("MixerDestroy()\n");
}

void
MixerPlay(Mixer* s, String sPath)
{
    MixerPause(s, false);

    guard::Mtx lock(&s->mtxAdd);
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

static void
MixerRunThread(Mixer* s, int argc, char** argv)
{
    pw_init(&argc, &argv);

    u8 setupBuffer[1024] {};
    const spa_pod* aParams[1] {};
    spa_pod_builder b = SPA_POD_BUILDER_INIT(setupBuffer, sizeof(setupBuffer));

    s->pThrdLoop = pw_thread_loop_new("runThread2", {});

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
        .rate = s->sampleRate,
        .channels = s->channels,
        .position {}
    };

    aParams[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    pw_stream_connect(
        s->pStream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT|PW_STREAM_FLAG_MAP_BUFFERS|PW_STREAM_FLAG_ASYNC),
        aParams,
        utils::size(aParams)
    );

    pw_stream_set_active(s->pStream, false);
    pw_thread_loop_start(s->pThrdLoop);
}

static void
writeFramesLocked(Mixer* s, f32* pBuff, u32 nFrames, long* pSamplesWritten)
{
    guard::Mtx lock(&s->mtxDecoder);

    if (!s->bDecodes) return;

    auto err = ffmpeg::DecoderWriteToBuffer(s->pDecoder, pBuff, utils::size(s_aPwBuff), nFrames, pSamplesWritten);
    if (err == ffmpeg::ERROR::EOF_)
    {
        MixerPause(s, true);
        s->bDecodes = false;
    }
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

    u32 stride = formatByteSize(s->eformat) * s->channels;
    u32 nFrames = pBuffData.maxsize / stride;
    if (pPwBuffer->requested) nFrames = SPA_MIN(pPwBuffer->requested, (u64)nFrames);

    if (nFrames * s->channels > utils::size(s_aPwBuff)) nFrames = utils::size(s_aPwBuff);

    s->nLastFrames = nFrames;

    static long nDecodedSamples = 0;
    static long nWrites = 0;

    f32 vol = std::pow(audio::g_globalVolume, 3);

    for (u32 frameIdx = 0; frameIdx < nFrames; frameIdx++)
    {
        for (u32 chIdx = 0; chIdx < s->channels; chIdx++)
        {
            /* modify each sample here */
            *pDest++ = s_aPwBuff[nWrites++] * vol;

            if (nWrites >= nDecodedSamples)
            {
                /* ask to fill the buffer when it's empty */
                writeFramesLocked(s, s_aPwBuff, nFrames, &nDecodedSamples);
                nWrites = 0;
            }
        }
    }

    pBuffData.chunk->offset = 0;
    pBuffData.chunk->stride = stride;
    pBuffData.chunk->size = nFrames * stride;

    pw_stream_queue_buffer(s->pStream, pPwBuffer);
}

[[maybe_unused]] static bool
MixerEmpty(Mixer* s)
{
    return true;
}

void
MixerPause(Mixer* s, bool bPause)
{
    ThrdLoopLockGuard lock(s->pThrdLoop);
    pw_stream_set_active(s->pStream, !bPause);
    s->base.bPaused = bPause;

    LOG_NOTIFY("bPaused: {}\n", s->base.bPaused);
}

void
MixerTogglePause(Mixer* s)
{
    MixerPause(s, !s->base.bPaused);
}

} /* namespace pipewire */
} /* namespace platform */

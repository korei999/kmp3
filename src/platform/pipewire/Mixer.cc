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

static void MixerRunThread(Mixer* s, int argc, char** argv);
static void onProcess(void* data);
static bool MixerEmpty(Mixer* s);

const pw_stream_events Mixer::s_streamEvents {
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

    struct Args
    {
        Mixer* s;
        int argc;
        char** argv;
    };

    static Args a {
        .s = s,
        .argc = app::g_argc,
        .argv = app::g_argv
    };

    auto fnp = +[](void* arg) -> int {
        auto a = *(Args*)arg;
        MixerRunThread(a.s, a.argc, a.argv);

        return thrd_success;
    };

    thrd_create(&s->threadLoop, fnp, &a);
    thrd_detach(s->threadLoop);
}

void
MixerDestroy(Mixer* s)
{
    mtx_destroy(&s->mtxAdd);
    mtx_destroy(&s->mtxDecoder);
    VecDestroy(&s->aTracks);
    VecDestroy(&s->aBackgroundTracks);
}

void
MixerAdd(Mixer* s, audio::Track t)
{
    guard::Mtx lock(&s->mtxAdd);

    if (VecSize(&s->aTracks) < audio::MAX_TRACK_COUNT) VecPush(&s->aTracks, t);
    else LOG_WARN("MAX_TRACK_COUNT({}) reached, ignoring track push\n", audio::MAX_TRACK_COUNT);
}

void
MixerAddBackground(Mixer* s, audio::Track t)
{
    guard::Mtx lock(&s->mtxAdd);

    if (VecSize(&s->aTracks) < audio::MAX_TRACK_COUNT) VecPush(&s->aBackgroundTracks, t);
    else LOG_WARN("MAX_TRACK_COUNT({}) reached, ignoring track push\n", audio::MAX_TRACK_COUNT);
}

void
MixerPlay(Mixer* s, String sPath)
{
    guard::Mtx lock(&s->mtxAdd);
    guard::Mtx lockDec(&s->mtxDecoder);

    if (!s->pDecoder)
    {
        s->pDecoder = ffmpeg::DecoderAlloc(app::g_pPlayer->pAlloc);
        if (!s->pDecoder)
            LOG_FATAL("DecoderAlloc\n");

        s->bPlaying = false;
    }

    if (s->bPlaying)
    {
        LOG_NOTIFY("Closing Decoder\n");
        s->bPlaying = false;
        utils::fill(s_aPwBuff, 0.0f, utils::size(s_aPwBuff));
        ffmpeg::DecoderClose(s->pDecoder);
    }

    ffmpeg::ERROR err {};
    /*if ((err = ffmpeg::DecoderOpen(s->pDecoder, sPath)) != ffmpeg::ERROR::OK)*/
    /*    LOG_FATAL("DecoderOpen: '{}'\n", ffmpeg::mapERRORToString[err]);*/

    s->bPlaying = true;
}

static void
MixerRunThread(Mixer* s, int argc, char** argv)
{
    pw_init(&argc, &argv);

    u8 setupBuffer[1024] {};
    const spa_pod* aParams[1] {};
    spa_pod_builder b = SPA_POD_BUILDER_INIT(setupBuffer, sizeof(setupBuffer));

    s->pLoop = pw_main_loop_new(nullptr);

    s->pStream = pw_stream_new_simple(
        pw_main_loop_get_loop(s->pLoop),
        "kmp3AudioSource",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "Music",
            nullptr
        ),
        &s->s_streamEvents,
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

    pw_main_loop_run(s->pLoop);

    pw_stream_destroy(s->pStream);
    pw_main_loop_destroy(s->pLoop);
    pw_deinit();
    s->base.bRunning = false;
}

static void
writeFrames(Mixer* s, f32* pBuff, u32 nFrames, long* pSamplesWritten)
{
    for (auto& t : s->aTracks)
    {
        auto err = ffmpeg::DecoderWriteToBuffer(t.pDecoder, pBuff, utils::size(s_aPwBuff), nFrames, pSamplesWritten);
        if (err == ffmpeg::ERROR::EOF_)
        {
            /* TODO: lock and pop or something */
        }

        break;
    }
}

static void
onProcess(void* data)
{
    auto* s = (Mixer*)data;

    guard::Mtx lock(&s->mtxAdd);

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

    s->lastNFrames = nFrames;

    static long nDecodedSamples = 0;
    static long nWrites = 0;
    for (u32 frameIdx = 0; frameIdx < nFrames; frameIdx++)
    {
        for (u32 chIdx = 0; chIdx < s->channels; chIdx++)
        {
            /*modify each sample here */
            *pDest++ = s_aPwBuff[nWrites++];

            if (nWrites >= nDecodedSamples)
            {
                /* ask to fill the buffer when it's empty */
                writeFrames(s, s_aPwBuff, nFrames, &nDecodedSamples);
                nWrites = 0;
            }
        }
    }

    pBuffData.chunk->offset = 0;
    pBuffData.chunk->stride = stride;
    pBuffData.chunk->size = nFrames * stride;

    pw_stream_queue_buffer(s->pStream, pPwBuffer);

    if (!app::g_bRunning) pw_main_loop_quit(s->pLoop);
    /* set bRunning for the mixer outside */
}

[[maybe_unused]] static bool
MixerEmpty(Mixer* s)
{
    bool r;
    {
        guard::Mtx lock(&s->mtxAdd);
        r = VecSize(&s->aTracks) > 0;
    }

    return r;
}

void
MixerPause(Mixer* s, bool bPause)
{
    /* FIXME: complains about calling from different thread, still works tho */
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

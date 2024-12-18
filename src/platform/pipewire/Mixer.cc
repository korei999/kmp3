#include "Mixer.hh"

#include "adt/guard.hh"
#include "adt/logs.hh"
#include "adt/utils.hh"
#include "adt/math.hh"
#include "app.hh"
#include "defaults.hh"
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

static void runThread(Mixer* s, int argc, char** argv);
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
Mixer::init()
{
    this->super.bRunning = true;
    this->super.bMuted = false;
    this->super.volume = 0.1f;

    this->super.sampleRate = 48000;
    this->nChannels = 2;
    this->eformat = SPA_AUDIO_FORMAT_F32;

    mtx_init(&this->mtxDecoder, mtx_plain);

    runThread(this, app::g_argc, app::g_argv);
}

void
Mixer::destroy()
{
    {
        ThrdLoopLockGuard tLock(this->pThrdLoop);
        pw_stream_set_active(this->pStream, true);
    }

    pw_thread_loop_stop(this->pThrdLoop);
    LOG_NOTIFY("pw_thread_loop_stop()\n");

    this->super.bRunning = false;

    if (this->bDecodes) ffmpeg::DecoderClose(this->pDecoder);

    pw_stream_destroy(this->pStream);
    pw_thread_loop_destroy(this->pThrdLoop);
    pw_deinit();

    mtx_destroy(&this->mtxDecoder);
    LOG_NOTIFY("MixerDestroy()\n");
}

void
Mixer::play(String sPath)
{
    const f64 prevSpeed = f64(this->super.changedSampleRate) / f64(this->super.sampleRate);

    this->pause(true);

    {
        guard::Mtx lockDec(&this->mtxDecoder);

        this->sPath = sPath;

        if (this->bDecodes)
        {
            ffmpeg::DecoderClose(this->pDecoder);
        }

        auto err = ffmpeg::DecoderOpen(this->pDecoder, sPath);
        if (err != ffmpeg::ERROR::OK_)
        {
            LOG_WARN("DecoderOpen\n");
            return;
        }
        this->bDecodes = true;
    }

    this->super.nChannels = ffmpeg::DecoderGetChannelsCount(this->pDecoder);
    this->changeSampleRate(ffmpeg::DecoderGetSampleRate(this->pDecoder), true);

    if (!math::eq(prevSpeed, 1.0))
        this->changeSampleRate(f64(this->super.sampleRate) * prevSpeed, false);

    this->pause(false);

    this->super.bUpdateMpris = true; /* mark to update in frame::run() */
}

static void
runThread(Mixer* s, int argc, char** argv)
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
        .rate = s->super.sampleRate,
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
            s->pause(true);
            ffmpeg::DecoderClose(s->pDecoder);
            s->bDecodes = false;
        }
    }

    if (err == ffmpeg::ERROR::END_OF_FILE) app::g_pPlayer->onSongEnd();
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

    f32 vol = s->super.bMuted ? 0.0f : std::pow(s->super.volume, 3);

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
            writeFramesLocked(s, s_aPwBuff, nFrames, &nDecodedSamples, &s->super.currentTimeStamp);
            nWrites = 0;
        }
    }

    if (nDecodedSamples == 0)
    {
        s->super.currentTimeStamp = 0;
        s->super.totalSamplesCount = 0;
    }
    else
    {
        s->super.totalSamplesCount = ffmpeg::DecoderGetTotalSamplesCount(s->pDecoder);
    }

    pBuffData.chunk->offset = 0;
    pBuffData.chunk->stride = stride;
    pBuffData.chunk->size = nFrames * stride;

    pw_stream_queue_buffer(s->pStream, pPwBuffer);
}

void
Mixer::pause(bool bPause)
{
    ThrdLoopLockGuard lock(this->pThrdLoop);
    pw_stream_set_active(this->pStream, !bPause);
    this->super.bPaused = bPause;

    LOG_NOTIFY("bPaused: {}\n", this->super.bPaused);
    mpris::playbackStatusChanged();
}

void
Mixer::togglePause()
{
    this->pause(!this->super.bPaused);
}

void
Mixer::changeSampleRate(u64 sampleRate, bool bSave)
{
    sampleRate = utils::clamp(sampleRate, defaults::MIN_SAMPLE_RATE, defaults::MAX_SAMPLE_RATE);

    u8 aSetupBuff[512] {};
    spa_audio_info_raw rawInfo {
        .format = this->eformat,
        .flags {},
        .rate = static_cast<u32>(sampleRate),
        .channels = this->nChannels,
        .position {}
    };

    spa_pod_builder b {};
    spa_pod_builder_init(&b, aSetupBuff, sizeof(aSetupBuff));

    const spa_pod* aParams[1] {};
    aParams[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    ThrdLoopLockGuard lock(this->pThrdLoop);
    pw_stream_update_params(this->pStream, aParams, utils::size(aParams));
    /* update won't apply without this */
    pw_stream_set_active(this->pStream, this->super.bPaused);
    pw_stream_set_active(this->pStream, !this->super.bPaused);

    if (bSave) this->super.sampleRate = sampleRate;

    this->super.changedSampleRate = sampleRate;
}

void
Mixer::seekMS(s64 ms)
{
    guard::Mtx lock(&this->mtxDecoder);
    if (!this->bDecodes) return;

    s64 maxMs = this->super.getMaxMS();
    ms = utils::clamp(ms, 0LL, maxMs);
    ffmpeg::DecoderSeekMS(this->pDecoder, ms);

    this->super.currentTimeStamp = f64(ms)/1000.0 * this->super.sampleRate * this->super.nChannels;
    this->super.totalSamplesCount = ffmpeg::DecoderGetTotalSamplesCount(this->pDecoder);

    mpris::seeked();
}

void
Mixer::seekLeftMS(s64 ms)
{
    u64 currMs = this->super.getCurrentMS();
    this->seekMS(currMs - ms);
}

void
Mixer::seekRightMS(s64 ms)
{
    u64 currMs = this->super.getCurrentMS();
    this->seekMS(currMs + ms);
}

Opt<String>
Mixer::getMetadata(const String sKey)
{
    return ffmpeg::DecoderGetMetadataValue(this->pDecoder, sKey);
}

Opt<ffmpeg::Image>
Mixer::getCoverImage()
{
    return ffmpeg::DecoderGetCoverImage(this->pDecoder);
}

void
Mixer::setVolume(const f32 volume)
{
    this->super.volume = utils::clamp(volume, 0.0f, defaults::MAX_VOLUME);
    mpris::volumeChanged();
}

} /* namespace pipewire */
} /* namespace platform */

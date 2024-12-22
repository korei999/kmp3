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

struct PWLockGuard
{
    pw_thread_loop* p;

    PWLockGuard() = delete;
    PWLockGuard(pw_thread_loop* _p) : p(_p) { pw_thread_loop_lock(_p); }
    ~PWLockGuard() { pw_thread_loop_unlock(p); }
};

void onProcess(void* data);

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

static f32 s_aPwBuff[audio::CHUNK_SIZE] {};

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
    m_bRunning = true;
    m_bMuted = false;
    m_volume = 0.1f;

    m_sampleRate = 48000;
    m_nChannels = 2;
    m_eformat = SPA_AUDIO_FORMAT_F32;

    mtx_init(&m_mtxDecoder, mtx_plain);

    runThread(this, app::g_argc, app::g_argv);
}

void
Mixer::destroy()
{
    {
        PWLockGuard tLock(m_pThrdLoop);
        pw_stream_set_active(m_pStream, true);
    }

    pw_thread_loop_stop(m_pThrdLoop);
    LOG_NOTIFY("pw_thread_loop_stop()\n");

    m_bRunning = false;

    if (m_bDecodes) m_pIReader->close();

    pw_stream_destroy(m_pStream);
    pw_thread_loop_destroy(m_pThrdLoop);
    pw_deinit();

    mtx_destroy(&m_mtxDecoder);
    LOG_NOTIFY("MixerDestroy()\n");
}

void
Mixer::play(String sPath)
{
    const f64 prevSpeed = f64(m_changedSampleRate) / f64(m_sampleRate);

    pause(true);

    {
        guard::Mtx lockDec(&m_mtxDecoder);

        m_sPath = sPath;

        if (m_bDecodes)
        {
            m_pIReader->close();
        }

        auto err = m_pIReader->open(sPath);
        if (err != audio::ERROR::OK_)
        {
            LOG_WARN("DecoderOpen\n");
            return;
        }
        m_bDecodes = true;
    }

    m_nChannels = m_pIReader->getChannelsCount();
    changeSampleRate(m_pIReader->getSampleRate(), true);

    if (!math::eq(prevSpeed, 1.0))
        changeSampleRate(f64(m_sampleRate) * prevSpeed, false);

    pause(false);

    m_bUpdateMpris = true; /* mark to update in frame::run() */
}

void
runThread(Mixer* s, int argc, char** argv)
{
    pw_init(&argc, &argv);

    u8 setupBuffer[1024] {};
    const spa_pod* aParams[1] {};
    spa_pod_builder b {};
    spa_pod_builder_init(&b, setupBuffer, sizeof(setupBuffer));

    s->m_pThrdLoop = pw_thread_loop_new("kmp3PwThreadLoop", {});

    s->m_pStream = pw_stream_new_simple(
        pw_thread_loop_get_loop(s->m_pThrdLoop),
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
        .format = s->m_eformat,
        .flags {},
        .rate = s->m_sampleRate,
        .channels = s->m_nChannels,
        .position {}
    };

    aParams[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    pw_stream_connect(
        s->m_pStream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT|PW_STREAM_FLAG_INACTIVE|PW_STREAM_FLAG_MAP_BUFFERS),
        aParams,
        utils::size(aParams)
    );

    pw_thread_loop_start(s->m_pThrdLoop);
}

void
writeFramesLocked(Mixer* s, f32* pBuff, u32 nFrames, long* pSamplesWritten, u64* pPcmPos)
{
    audio::ERROR err {};
    {
        guard::Mtx lock(&s->m_mtxDecoder);

        if (!s->m_bDecodes) return;

        err = s->m_pIReader->writeToBuffer(
            pBuff, utils::size(s_aPwBuff),
            nFrames, s->m_nChannels,
            pSamplesWritten, pPcmPos
        );
        if (err == audio::ERROR::END_OF_FILE)
        {
            s->pause(true);
            s->m_pIReader->close();
            s->m_bDecodes = false;
        }
    }

    if (err == audio::ERROR::END_OF_FILE) app::g_pPlayer->onSongEnd();
}

void
onProcess(void* pData)
{
    auto* self = (Mixer*)pData;

    pw_buffer* pPwBuffer = pw_stream_dequeue_buffer(self->m_pStream);
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

    u32 stride = formatByteSize(self->m_eformat) * self->m_nChannels;
    u32 nFrames = pBuffData.maxsize / stride;
    if (pPwBuffer->requested) nFrames = SPA_MIN(pPwBuffer->requested, (u64)nFrames);

    if (nFrames * self->m_nChannels > utils::size(s_aPwBuff)) nFrames = utils::size(s_aPwBuff);

    self->m_nLastFrames = nFrames;

    static long s_nDecodedSamples = 0;
    static long s_nWrites = 0;

    f32 vol = self->m_bMuted ? 0.0f : std::pow(self->m_volume, 3);

    for (u32 frameIdx = 0; frameIdx < nFrames; frameIdx++)
    {
        for (u32 chIdx = 0; chIdx < self->m_nChannels; chIdx++)
        {
            /* modify each sample here */
            *pDest++ = s_aPwBuff[s_nWrites++] * vol;
        }

        if (s_nWrites >= s_nDecodedSamples)
        {
            /* ask to fill the buffer when it's empty */
            writeFramesLocked(self, s_aPwBuff, nFrames, &s_nDecodedSamples, &self->m_currentTimeStamp);
            s_nWrites = 0;
        }
    }

    if (s_nDecodedSamples == 0)
    {
        self->m_currentTimeStamp = 0;
        self->m_totalSamplesCount = 0;
    }
    else
    {
        self->m_totalSamplesCount = self->m_pIReader->getTotalSamplesCount();
    }

    pBuffData.chunk->offset = 0;
    pBuffData.chunk->stride = stride;
    pBuffData.chunk->size = nFrames * stride;

    pw_stream_queue_buffer(self->m_pStream, pPwBuffer);
}

void
Mixer::pause(bool bPause)
{
    PWLockGuard lock(m_pThrdLoop);
    pw_stream_set_active(m_pStream, !bPause);
    m_bPaused = bPause;

    LOG_NOTIFY("bPaused: {}\n", m_bPaused);
    mpris::playbackStatusChanged();
}

void
Mixer::togglePause()
{
    pause(!m_bPaused);
}

void
Mixer::changeSampleRate(u64 sampleRate, bool bSave)
{
    sampleRate = utils::clamp(sampleRate, defaults::MIN_SAMPLE_RATE, defaults::MAX_SAMPLE_RATE);

    u8 aSetupBuff[512] {};
    spa_audio_info_raw rawInfo {
        .format = m_eformat,
        .flags {},
        .rate = static_cast<u32>(sampleRate),
        .channels = m_nChannels,
        .position {}
    };

    spa_pod_builder b {};
    spa_pod_builder_init(&b, aSetupBuff, sizeof(aSetupBuff));

    const spa_pod* aParams[1] {};
    aParams[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    PWLockGuard lock(m_pThrdLoop);
    pw_stream_update_params(m_pStream, aParams, utils::size(aParams));
    /* update won't apply without this */
    pw_stream_set_active(m_pStream, m_bPaused);
    pw_stream_set_active(m_pStream, !m_bPaused);

    if (bSave) m_sampleRate = sampleRate;

    m_changedSampleRate = sampleRate;
}

void
Mixer::seekMS(s64 ms)
{
    guard::Mtx lock(&m_mtxDecoder);
    if (!m_bDecodes) return;

    s64 maxMs = getMaxMS();
    ms = utils::clamp(ms, 0LL, maxMs);
    m_pIReader->seekMS(ms);

    m_currentTimeStamp = f64(ms)/1000.0 * m_sampleRate * m_nChannels;
    m_totalSamplesCount = m_pIReader->getTotalSamplesCount();

    mpris::seeked();
}

void
Mixer::seekLeftMS(s64 ms)
{
    u64 currMs = getCurrentMS();
    seekMS(currMs - ms);
}

void
Mixer::seekRightMS(s64 ms)
{
    u64 currMs = getCurrentMS();
    seekMS(currMs + ms);
}

Opt<String>
Mixer::getMetadata(const String sKey)
{
    return m_pIReader->getMetadataValue(sKey);
}

Opt<Image>
Mixer::getCoverImage()
{
    return m_pIReader->getCoverImage();
}

void
Mixer::setVolume(const f32 volume)
{
    m_volume = utils::clamp(volume, 0.0f, defaults::MAX_VOLUME);
    mpris::volumeChanged();
}

} /* namespace pipewire */
} /* namespace platform */

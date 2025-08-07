#include "Mixer.hh"

#include "app.hh"
#include "defaults.hh"
#include "mpris.hh"

#include <cmath>

using namespace adt;

namespace platform::pipewire
{

struct PWLockGuard
{
    pw_thread_loop* p {};

    PWLockGuard() = delete;
    PWLockGuard(pw_thread_loop* _p) : p(_p) { pw_thread_loop_lock(p); }
    ~PWLockGuard() { pw_thread_loop_unlock(p); }
};

static const pw_stream_events s_streamEvents {
    .version = PW_VERSION_STREAM_EVENTS,
    .destroy {},
    .state_changed {},
    .control_info {},
    .io_changed {},
    .param_changed {},
    .add_buffer {},
    .remove_buffer {},
    .process = decltype(pw_stream_events::process)(Mixer::getOnProcessPFN()),
    .drained {},
    .command {},
    .trigger_done {},
};

static u32
formatByteSize(spa_audio_format eFormat)
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

Mixer&
Mixer::init()
{
    m_bRunning = true;
    m_bMuted = false;
    m_volume = 0.1f;

    m_sampleRate = 48000;
    m_nChannels = 2;
    m_eformat = SPA_AUDIO_FORMAT_F32;

    pw_init({}, {});

    u8 aSetupBuffer[1024] {};
    const spa_pod* aParams[1] {};
    spa_pod_builder b {};
    spa_pod_builder_init(&b, aSetupBuffer, sizeof(aSetupBuffer));

    m_pThrdLoop = pw_thread_loop_new("kmp3PwThreadLoop", {});

    m_pStream = pw_stream_new_simple(
        pw_thread_loop_get_loop(m_pThrdLoop),
        "kmp3AudioSource",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "Music",
            nullptr
        ),
        &s_streamEvents,
        this
    );

    spa_audio_info_raw rawInfo {
        .format = m_eformat,
        .flags {},
        .rate = m_sampleRate,
        .channels = m_nChannels,
        .position {}
    };

    aParams[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    pw_stream_connect(
        m_pStream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(
            PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE | PW_STREAM_FLAG_MAP_BUFFERS
        ),
        aParams,
        utils::size(aParams)
    );

    pw_thread_loop_start(m_pThrdLoop);

    return *this;
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

    if (m_atom_bDecodes.load(atomic::ORDER::RELAXED)) app::decoder().close();

    if (m_pStream) pw_stream_destroy(m_pStream);
    if (m_pThrdLoop) pw_thread_loop_destroy(m_pThrdLoop);
    pw_deinit();

    LOG_BAD("MixerDestroy()\n");
}

bool
Mixer::play(StringView svPath)
{
    const f64 prevSpeed = f64(m_changedSampleRate) / f64(m_sampleRate);

    pause(true);

    {
        LockGuard lockDec {&app::decoder().m_mtx};

        if (m_atom_bDecodes.load(atomic::ORDER::RELAXED)) app::decoder().close();

        if (audio::ERROR err = app::decoder().open(svPath);
            err != audio::ERROR::OK_
        )
        {
            return false;
        }

        m_atom_bDecodes.store(true, atomic::ORDER::RELAXED);
    }

    setNChannles(app::decoder().getChannelsCount());
    changeSampleRate(app::decoder().getSampleRate(), true);

    if (!math::eq(prevSpeed, 1.0))
        changeSampleRate(f64(m_sampleRate) * prevSpeed, false);

    pause(false);

#ifdef OPT_MPRIS
    m_atom_bUpdateMpris.store(true, atomic::ORDER::RELEASE); /* mark to update in frame::run() */
#endif

    return true;
}

void
Mixer::setNChannles(u32 nChannles)
{
    m_nChannels = nChannles;

    u8 aSetupBuff[512] {};
    spa_audio_info_raw rawInfo {
        .format = m_eformat,
        .flags {},
        .rate = m_sampleRate,
        .channels = nChannles,
        .position {}
    };

    spa_pod_builder b {};
    spa_pod_builder_init(&b, aSetupBuff, sizeof(aSetupBuff));

    const spa_pod* pParams {};
    pParams = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    PWLockGuard lock(m_pThrdLoop);
    pw_stream_update_params(m_pStream, &pParams, 1);

    /* won't apply without this */
    {
        bool bPaused = m_atom_bPaused.load(atomic::ORDER::ACQUIRE);
        pw_stream_set_active(m_pStream, bPaused);
        pw_stream_set_active(m_pStream, !bPaused);
    }
}

void
Mixer::onProcess()
{
    pw_buffer* pPwBuffer = pw_stream_dequeue_buffer(m_pStream);
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

    u32 stride = formatByteSize(m_eformat) * m_nChannels;
    u32 nFrames = (stride > 0) ? (pBuffData.maxsize / stride) : 0;
    if (pPwBuffer->requested) nFrames = SPA_MIN(pPwBuffer->requested, (u64)nFrames);

    if (nFrames*m_nChannels > utils::size(audio::g_aRenderBuffer)) nFrames = utils::size(audio::g_aRenderBuffer);

    static long s_nDecodedSamples = 0;
    static long s_nWrites = 0;

    const f32 vol = m_bMuted ? 0.0f : std::pow(m_volume, 3.0f);

    isize destI = 0;
    for (u32 frameIdx = 0; frameIdx < nFrames; ++frameIdx)
    {
        /* fill the buffer when it's empty */
        if (s_nWrites >= s_nDecodedSamples)
        {
            writeFramesLocked({audio::g_aRenderBuffer}, nFrames, &s_nDecodedSamples, &m_currentTimeStamp);

            m_currMs = app::decoder().getCurrentMS();
            s_nWrites = 0;
        }

        for (u32 chIdx = 0; chIdx < m_nChannels; ++chIdx)
        {
            /* modify each sample here */
            pDest[destI++] = audio::g_aRenderBuffer[s_nWrites++] * vol;
        }
    }

    if (s_nDecodedSamples == 0)
    {
        m_currentTimeStamp = 0;
        m_nTotalSamples = 0;
    }
    else
    {
        m_nTotalSamples = app::decoder().getTotalSamplesCount();
    }

    pBuffData.chunk->offset = 0;
    pBuffData.chunk->stride = stride;
    pBuffData.chunk->size = nFrames * stride;

    pw_stream_queue_buffer(m_pStream, pPwBuffer);
}

void
Mixer::pause(bool bPause)
{
    PWLockGuard lock(m_pThrdLoop);
    pw_stream_set_active(m_pStream, !bPause);
    m_atom_bPaused.store(bPause, atomic::ORDER::RELEASE);

    LOG_NOTIFY("bPaused: {}\n", m_atom_bPaused);
    mpris::playbackStatusChanged();
}

void
Mixer::togglePause()
{
    pause(!m_atom_bPaused.load(atomic::ORDER::ACQUIRE));
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
    {
        bool bPaused = m_atom_bPaused.load(atomic::ORDER::ACQUIRE);
        pw_stream_set_active(m_pStream, bPaused);
        pw_stream_set_active(m_pStream, !bPaused);
    }

    if (bSave) m_sampleRate = sampleRate;

    m_changedSampleRate = sampleRate;
}

void
Mixer::seekMS(f64 ms)
{
    {
        LockGuard lock {&app::decoder().m_mtx};

        if (!m_atom_bDecodes.load(atomic::ORDER::ACQUIRE)) return;

        ms = utils::clamp(ms, 0.0, f64(app::decoder().getTotalMS()));
        app::decoder().seekMS(ms);

        m_currMs = ms;
        m_currentTimeStamp = (ms * m_sampleRate * m_nChannels) / 1000.0;
        m_nTotalSamples = app::decoder().getTotalSamplesCount();
    }

    mpris::seeked();
}

void
Mixer::seekOff(f64 offset)
{
    auto time = app::decoder().getCurrentMS() + offset;
    seekMS(time);
}

void
Mixer::setVolume(const f32 volume)
{
    m_volume = utils::clamp(volume, 0.0f, defaults::MAX_VOLUME);
    mpris::volumeChanged();
}

i64
Mixer::getCurrentMS()
{
    return m_currMs;
}

i64
Mixer::getTotalMS()
{
    LockGuard lock {&app::decoder().m_mtx};
    return app::decoder().getTotalMS();
}

} /* namespace platform::pipewire */

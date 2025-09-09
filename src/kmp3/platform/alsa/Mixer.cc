#include "Mixer.hh"

#include "app.hh"
#include "platform/mpris/mpris.hh"

using namespace adt;

namespace platform::alsa
{

static const char* s_pDevice = "default";

Mixer&
Mixer::init()
{
    LogInfo("initializing alsa...\n");

    int err = 0;

    ADT_RUNTIME_EXCEPTION_FMT(
        (err = snd_pcm_open(&m_pHandle, s_pDevice, SND_PCM_STREAM_PLAYBACK, 0)) >= 0,
        "({}): {}", err, snd_strerror(err)
    );

    ADT_RUNTIME_EXCEPTION_FMT(
        (err = snd_pcm_set_params(
            m_pHandle,
            SND_PCM_FORMAT_FLOAT,
            SND_PCM_ACCESS_RW_INTERLEAVED,
            2, 48000, 1, 100000
        )) >= 0,
        "({}): {}", err, snd_strerror(err)
    );

    m_atom_bRunning.store(true, atomic::ORDER::RELEASE);

    new(&m_thrdLoop) Thread {
        ThreadFn(methodPointerNonVirtual(&Mixer::loop)),
        this
    };

    return *this;
}

void
Mixer::destroy()
{
    m_atom_bRunning.store(false, atomic::ORDER::RELEASE);

    while (m_atom_bLoopDone.load(atomic::ORDER::ACQUIRE) != true)
        m_cndLoop.signal();

    m_thrdLoop.join();
    m_mtxLoop.destroy();
    m_cndLoop.destroy();

    // snd_pcm_drain(m_pHandle); /* Too slow. */
    snd_pcm_close(m_pHandle);

    LogDebug("MixerDestroy()\n");
}

bool
Mixer::play(StringView svPath)
{
    const f64 prevSpeed = f64(m_changedSampleRate) / f64(m_sampleRate);

    pause(true);

    {
        LockGuard lockDec {&app::decoder().m_mtx};

        if (m_atom_bDecodes.load(atomic::ORDER::ACQUIRE)) app::decoder().close();

        if (audio::ERROR err = app::decoder().open(svPath);
            err != audio::ERROR::OK_
        )
        {
            return false;
        }

        m_atom_bDecodes.store(true, atomic::ORDER::RELAXED);
    }

    setConfig(app::decoder().getSampleRate(), app::decoder().getChannelsCount(), true);

    if (!math::eq(prevSpeed, 1.0))
        changeSampleRate(f64(m_sampleRate) * prevSpeed, false);

    pause(false);

    return true;
}

void
Mixer::pause(bool bPause)
{
    LockGuard lock {&m_mtxLoop};

    defer(
        m_cndLoop.signal();
        mpris::playbackStatusChanged()
    );

    bool bCurr = m_atom_bPaused.load(atomic::ORDER::ACQUIRE);
    if (bCurr == bPause) return;

    LogInfo("bPause: {}\n", bPause);
    m_atom_bPaused.store(bPause, atomic::ORDER::RELEASE);

    if (bPause) snd_pcm_pause(m_pHandle, true);
    else snd_pcm_pause(m_pHandle, false);
}

void
Mixer::togglePause()
{
    pause(!m_atom_bPaused.load(atomic::ORDER::ACQUIRE));
}

void
Mixer::changeSampleRate(u64 sampleRate, bool bSave)
{
    int err = 0;

    pause(true);
    ADT_RUNTIME_EXCEPTION_FMT(
        (err = snd_pcm_set_params(
            m_pHandle,
            SND_PCM_FORMAT_FLOAT,
            SND_PCM_ACCESS_RW_INTERLEAVED,
            m_nChannels, sampleRate, 1, 250000
        )) >= 0,
        "({}): {}", err, snd_strerror(err)
    );
    pause(false);

    if (bSave) m_sampleRate = sampleRate;
    m_changedSampleRate = sampleRate;
}

void
Mixer::seekMS(f64 ms)
{
    LockGuard lock {&app::decoder().m_mtx};

    if (!m_atom_bDecodes.load(atomic::ORDER::ACQUIRE)) return;

    ms = utils::clamp(ms, 0.0, f64(app::decoder().getTotalMS()));
    app::decoder().seekMS(ms);

    m_currMs = ms;
    m_currentTimeStamp = (ms * m_sampleRate * m_nChannels) / 1000.0;
    m_nTotalSamples = app::decoder().getTotalSamplesCount();

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
    m_volume = utils::clamp(volume, 0.0f, app::g_config.maxVolume);
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

void
Mixer::setConfig(u64 sampleRate, int nChannels, bool bSaveNewConfig)
{
    LockGuard lock {&m_mtxLoop};
    int err = 0;

    if (bSaveNewConfig)
    {
        m_changedSampleRate = m_sampleRate = sampleRate;
        m_nChannels = nChannels;
    }

    sampleRate = utils::clamp(sampleRate, app::g_config.minSampleRate, app::g_config.maxSampleRate);
    nChannels = utils::clamp(nChannels, 1, 20);

    ADT_RUNTIME_EXCEPTION_FMT(
        (err = snd_pcm_set_params(
            m_pHandle,
            SND_PCM_FORMAT_FLOAT,
            SND_PCM_ACCESS_RW_INTERLEAVED,
            nChannels, sampleRate, 1, 100000
        )) >= 0,
        "({}): {}", err, snd_strerror(err)
    );
}

THREAD_STATUS
Mixer::loop()
{
    defer( m_atom_bLoopDone.store(true, atomic::ORDER::RELEASE) );

    StdAllocator stdAl;
    f32* pRenderBuff = stdAl.zallocV<f32>(utils::size(audio::g_aRenderBuffer));
    defer( stdAl.free(pRenderBuff) );

    long nDecodedSamples = 0;
    long nWrites = 0;

    constexpr isize NFRAMES = 2048;

    const isize ringSize = m_ringBuff.pop({audio::g_aRenderBuffer, NFRAMES * m_nChannels});
    m_currMs = app::decoder().getCurrentMS();
    nDecodedSamples = NFRAMES*m_nChannels;

    while (m_atom_bRunning.load(atomic::ORDER::ACQUIRE))
    {
        const f32 vol = m_bMuted ? 0.0f : std::pow(m_volume, 3.0f);

        isize destI = 0;
        nWrites = 0;

        for (isize i = 0; i < nDecodedSamples; ++i)
            pRenderBuff[destI++] = audio::g_aRenderBuffer[nWrites++] * vol;

        if (nDecodedSamples == 0)
        {
            m_currentTimeStamp = 0;
            m_nTotalSamples = 0;
        }
        else
        {
            m_nTotalSamples = app::decoder().getTotalSamplesCount();
        }

        {
            LockGuard lock {&m_mtxLoop};
            while (m_atom_bPaused.load(atomic::ORDER::ACQUIRE))
            {
                m_cndLoop.wait(&m_mtxLoop);

                if (!m_atom_bRunning.load(atomic::ORDER::ACQUIRE))
                    goto GOTO_done;
            }

            auto nFrameWritten = snd_pcm_writei(m_pHandle, pRenderBuff, NFRAMES);
            if (nFrameWritten < 0) nFrameWritten = snd_pcm_recover(m_pHandle, nFrameWritten, 0);
            if (nFrameWritten < 0)
            {
                LogError("snd_pcm_recover() failed: {}\n", snd_strerror(nFrameWritten));
                app::quit();
                goto GOTO_done;
            }
        }
    }

GOTO_done:
    LogDebug("LOOP DONE\n");

    return THREAD_STATUS(0);
}

} /* namespace platform::alsa */

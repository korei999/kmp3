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
            2, 48000, 1, 50000
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
Mixer::deinit()
{
    m_atom_bRunning.store(false, atomic::ORDER::RELEASE);
    pause(false); /* Sleeps in the loop() forever if paused. */

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

    if (!playFinal(svPath)) return false;

    setConfig(app::decoder().getSampleRate(), app::decoder().getChannelsCount(), true);

    if (!math::eq(prevSpeed, 1.0))
        changeSampleRate(f64(m_sampleRate) * prevSpeed, false);

    pause(false);

    return true;
}

void
Mixer::pause(bool bPause)
{
    bool bCurr = m_atom_bPaused.load(atomic::ORDER::ACQUIRE);
    if (bCurr == bPause) return;

    LogInfo("bPause: {}\n", bPause);
    m_atom_bPaused.store(bPause, atomic::ORDER::RELEASE);

    LockScope lock {&m_mtxLoop};

    if (bPause)
    {
        snd_pcm_pause(m_pHandle, true);
    }
    else
    {
        m_cndLoop.signal();
        snd_pcm_pause(m_pHandle, false);
    }

    mpris::playbackStatusChanged();
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
            m_nChannels, sampleRate, 1, 50000
        )) >= 0,
        "({}): {}", err, snd_strerror(err)
    );
    pause(false);

    if (bSave) m_sampleRate = sampleRate;
    m_changedSampleRate = sampleRate;
}

void
Mixer::setConfig(u64 sampleRate, int nChannels, bool bSaveNewConfig)
{
    LockScope lock {&m_mtxLoop};
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
            nChannels, sampleRate, 1, 50000
        )) >= 0,
        "({}): {}", err, snd_strerror(err)
    );
}

THREAD_STATUS
Mixer::loop()
{
    defer( m_atom_bLoopDone.store(true, atomic::ORDER::RELEASE) );

    constexpr isize NFRAMES = 2048;
    isize nSamplesRequested = 0;

    while (m_atom_bRunning.load(atomic::ORDER::ACQUIRE))
    {
        const f32 vol = m_bMuted ? 0.0f : std::pow(m_volume * (1.0f/100.0f), 3.0f);

        nSamplesRequested = NFRAMES * m_nChannels;
        m_ringBuff.pop({audio::g_aDrainBuffer, nSamplesRequested});

        for (isize sampleI = 0; sampleI < nSamplesRequested; ++sampleI)
            audio::g_aDrainBuffer[sampleI] *= vol;

        {
            LockScope lock {&m_mtxLoop};
            while (m_atom_bPaused.load(atomic::ORDER::ACQUIRE))
            {
                m_cndLoop.wait(&m_mtxLoop);

                if (!m_atom_bRunning.load(atomic::ORDER::ACQUIRE))
                    goto GOTO_done;
            }

            snd_pcm_sframes_t writeStatus = snd_pcm_writei(m_pHandle, audio::g_aDrainBuffer, NFRAMES);

            if (writeStatus < 0)
            {
                writeStatus = snd_pcm_recover(m_pHandle, writeStatus, 0);

                if (writeStatus < 0)
                {
                    LogError("snd_pcm_recover() failed: {}\n", snd_strerror(writeStatus));
                    app::quit();
                    goto GOTO_done;
                }
            }
        }
    }

GOTO_done:
    LogDebug("LOOP DONE\n");

    return THREAD_STATUS(0);
}

} /* namespace platform::alsa */

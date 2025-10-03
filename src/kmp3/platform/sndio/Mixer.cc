#include "Mixer.hh"

#include "app.hh"
#include "platform/mpris/mpris.hh"

namespace platform::sndio
{

static constexpr isize N_BUF_FRAMES = 1024;

void
Mixer::setConfig(u64 sampleRate, int nChannels, bool bSaveNewConfig)
{
    LockScope lock {&m_mtxLoop};

    if (bSaveNewConfig)
    {
        m_changedSampleRate = m_sampleRate = sampleRate;
        m_nChannels = nChannels;
    }

    sampleRate = utils::clamp(sampleRate, app::g_config.minSampleRate, app::g_config.maxSampleRate);
    nChannels = utils::clamp(nChannels, 1, 20);

    m_par.pchan = nChannels;
    m_par.rate = sampleRate;

    sio_setpar(m_pHdl, &m_par);
}

THREAD_STATUS
Mixer::loop()
{
    [[maybe_unused]] int writeStatus {};

    defer( m_atom_bLoopDone.store(true, atomic::ORDER::RELEASE) );

    /* sndio doesn't support f32 pcm. */
    i16* pRenderBuffer = Gpa::inst()->zallocV<i16>(utils::size(audio::g_aDrainBuffer));
    defer( Gpa::inst()->free(pRenderBuffer) );

    while (m_atom_bRunning.load(atomic::ORDER::ACQUIRE))
    {
        const f32 vol = m_bMuted ? 0.0f : std::pow(m_volume * (1.0f/100.0f), 3.0f);

        const long nSamplesRequested = N_BUF_FRAMES * m_nChannels;
        m_ringBuff.pop({audio::g_aDrainBuffer, nSamplesRequested});

        for (isize i = 0; i < nSamplesRequested; ++i)
        {
            const f32 sample = utils::clamp(
                std::numeric_limits<i16>::max() * (audio::g_aDrainBuffer[i] * vol),
                (f32)std::numeric_limits<i16>::min(),
                (f32)std::numeric_limits<i16>::max()
            );
            pRenderBuffer[i] = static_cast<i16>(sample);
        }

        {
            LockScope lock {&m_mtxLoop};
            while (m_atom_bPaused.load(atomic::ORDER::ACQUIRE))
            {
                m_cndLoop.wait(&m_mtxLoop);

                if (!m_atom_bRunning.load(atomic::ORDER::ACQUIRE))
                    goto GOTO_done;
            }

            writeStatus = sio_write(m_pHdl, pRenderBuffer, N_BUF_FRAMES * m_par.pchan * sizeof(i16));
        }
    }

GOTO_done:
    LogDebug("LOOP DONE\n");

    return THREAD_STATUS(0);
}

Mixer&
Mixer::init()
{
    LogDebug("initializing sndio...\n");

    m_bRunning = true;
    m_bMuted = false;

    constexpr u32 sampleRate = 48000;
    constexpr u32 nChannels = 2;
    constexpr u32 bitsPerSample = 16;
    constexpr u32 bytesPerSample = bitsPerSample / 8;

    ADT_RUNTIME_EXCEPTION(m_pHdl = sio_open(SIO_DEVANY, SIO_PLAY, 0));

    sio_initpar(&m_par);
    m_par.bits = bitsPerSample;
    m_par.bps = bytesPerSample;
    m_par.sig = 1;
    m_par.le = 1;
    m_par.pchan = nChannels;
    m_par.rchan = 0;
    m_par.rate = sampleRate;
    m_par.appbufsz = N_BUF_FRAMES;

    m_par.xrun = SIO_IGNORE;

#define _TRY(FN, ...)                                                                                                  \
    if (!FN(__VA_ARGS__))                                                                                              \
    {                                                                                                                  \
        sio_close(m_pHdl);                                                                                             \
        throw RuntimeException(#FN); \
    }

    m_atom_bRunning.store(true, atomic::ORDER::RELEASE);

    _TRY(sio_setpar, m_pHdl, &m_par);
    _TRY(sio_start, m_pHdl);

#undef _TRY

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

    sio_close(m_pHdl);

    LogDebug("MixerDestroy()\n");
}

bool
Mixer::play(StringView svPath)
{
    const f64 prevSpeed = f64(m_changedSampleRate) / f64(m_sampleRate);

    pause(true);
    if (!playFinal(svPath)) return false;

    setConfig(app::decoder().getSampleRate(), app::decoder().getChannelsCount(), true);

    if (!utils::floatEq(prevSpeed, 1.0))
        changeSampleRate(f64(m_sampleRate) * prevSpeed, false);

    pause(false);

    return true;
}

void
Mixer::pause(bool bPause)
{
    bool bCurr = m_atom_bPaused.load(atomic::ORDER::ACQUIRE);
    if (bCurr == bPause) return;

    m_atom_bPaused.store(bPause, atomic::ORDER::RELEASE);

    LockScope lock {&m_mtxLoop};

    if (bPause)
    {
        sio_stop(m_pHdl);
    }
    else
    {
        m_cndLoop.signal();
        sio_start(m_pHdl);
    }

    mpris::playbackStatusChanged();
}

void
Mixer::changeSampleRate(u64 sampleRate, bool bSave)
{
    pause(true);
    setConfig(sampleRate, m_nChannels, false);
    pause(false);

    if (bSave) m_sampleRate = sampleRate;
    m_changedSampleRate = sampleRate;
}

} /* namespace platform::sndio */

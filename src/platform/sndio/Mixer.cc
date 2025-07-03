#include "Mixer.hh"

#include "app.hh"
#include "mpris.hh"

#include "adt/defer.hh"
#include "adt/logs.hh"
#include "adt/math.hh"

using namespace adt;

namespace platform::sndio
{

static constexpr isize N_BUF_FRAMES = 1024 * 2;

void
Mixer::setConfig(u64 sampleRate, int nChannels, bool bSaveNewConfig)
{
    LockGuard lock {&m_mtxLoop};

    if (bSaveNewConfig)
    {
        m_changedSampleRate = m_sampleRate = sampleRate;
        m_nChannels = nChannels;
    }

    sampleRate = utils::clamp(sampleRate, defaults::MIN_SAMPLE_RATE, defaults::MAX_SAMPLE_RATE);
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

    StdAllocator stdAl;
    /* sndio doesn't support f32 pcm. */
    i16* pRenderBuffer = stdAl.zallocV<i16>(audio::CHUNK_SIZE);
    defer( stdAl.free(pRenderBuffer) );

    long s_nDecodedSamples = 0;
    long s_nWrites = 0;

    while (m_atom_bRunning.load(atomic::ORDER::ACQUIRE))
    {
        const f32 vol = m_bMuted ? 0.0f : std::pow(m_volume, 3.0f);

        isize destI = 0;
        for (u32 frameIdx = 0; frameIdx < N_BUF_FRAMES; ++frameIdx)
        {
            /* fill the buffer when it's empty */
            if (s_nWrites >= s_nDecodedSamples)
            {
                writeFramesLocked(audio::g_aRenderBuffer, N_BUF_FRAMES, &s_nDecodedSamples, &m_currentTimeStamp);

                m_currMs = app::decoder().getCurrentMS();
                s_nWrites = 0;
            }

            for (u32 chIdx = 0; chIdx < m_par.pchan; ++chIdx)
            {
                /* Convert to signed 16bit. */
                const i16 sample = std::numeric_limits<i16>::max() * (audio::g_aRenderBuffer[s_nWrites++] * vol);
                pRenderBuffer[destI++] = sample;
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

        {
            LockGuard lock {&m_mtxLoop};
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
    LOG_BAD("LOOP DONE\n");

    return THREAD_STATUS(0);
}

void
Mixer::init()
{
    LOG_NOTIFY("initializing sndio...\n");

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

    sio_close(m_pHdl);

    LOG_BAD("MixerDestroy()\n");
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

    m_atom_bPaused.store(bPause, atomic::ORDER::RELEASE);

    if (bPause) sio_stop(m_pHdl);
    else sio_start(m_pHdl);
}

void
Mixer::togglePause()
{
    pause(!m_atom_bPaused.load(atomic::ORDER::ACQUIRE));
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

} /* namespace platform::sndio */

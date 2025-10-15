#include "Mixer.hh"

#include "app.hh"
#include "platform/mpris/mpris.hh"

namespace platform::alsa
{

constexpr auto ntsDEVICE = "default";

static void
errorHandler(const char* pFile, int line, const char* pFunc, int, const char* pFmt, ...)
{
    va_list args;
    char aBuff[256];
    va_start(args, pFmt);
    isize n = vsnprintf(aBuff, sizeof(aBuff), pFmt, args);
    va_end(args);

    LogError{"({}, {}, {}): {}\n", pFile, line, pFunc, StringView{aBuff, n}};
}

int
Mixer::setHwParams(snd_pcm_hw_params_t* params, snd_pcm_access_t access)
{
    unsigned int rrate;
    snd_pcm_uframes_t size;
    int err, dir;

    /* choose all parameters */
    err = snd_pcm_hw_params_any(m_pHandle, params);
    if (err < 0)
    {
        LogError("Broken configuration for playback: no configurations available: {}\n", snd_strerror(err));
        return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(m_pHandle, params, 1);
    if (err < 0)
    {
        LogError("Resampling setup failed for playback: {}\n", snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(m_pHandle, params, access);
    if (err < 0)
    {
        LogError("Access type not available for playback: {}\n", snd_strerror(err));
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(m_pHandle, params, SND_PCM_FORMAT_FLOAT);
    if (err < 0)
    {
        LogError("Sample format not available for playback: {}\n", snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(m_pHandle, params, m_nChannels);
    if (err < 0)
    {
        LogError("Channels count ({}) not available for playbacks: {}\n", m_nChannels, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = m_sampleRate;
    err = snd_pcm_hw_params_set_rate_near(m_pHandle, params, &rrate, 0);
    if (err < 0)
    {
        LogError("Rate {}Hz not available for playback: {}\n", m_sampleRate, snd_strerror(err));
        return err;
    }
    if (rrate != m_sampleRate)
    {
        LogError("Rate doesn't match (requested {}Hz, get {}Hz)\n", m_sampleRate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(m_pHandle, params, &m_bufferTime, &dir);
    if (err < 0)
    {
        LogError("Unable to set buffer time {} for playback: {}\n", m_bufferTime, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &size);
    if (err < 0)
    {
        LogError("Unable to get buffer size for playback: {}\n", snd_strerror(err));
        return err;
    }
    m_bufferSize = size;
    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(m_pHandle, params, &m_periodTime, &dir);
    if (err < 0)
    {
        LogError("Unable to set period time {} for playback: %s\n", m_periodTime, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
    if (err < 0)
    {
        LogError("Unable to get period size for playback: {}\n", snd_strerror(err));
        return err;
    }
    m_periodSize = size;
    LogDebug{"pediodSize: {}\n", m_periodSize};
    /* write the parameters to device */
    err = snd_pcm_hw_params(m_pHandle, params);
    if (err < 0)
    {
        LogError("Unable to set hw params for playback: {}\n", snd_strerror(err));
        return err;
    }
    return 0;
}

int
Mixer::setSwParams(snd_pcm_sw_params_t* swparams)
{
    int err;
    int periodEvent = 0;                /* produce poll event after each period */

    /* get the current swparams */
    err = snd_pcm_sw_params_current(m_pHandle, swparams);
    if (err < 0)
    {
        LogError("Unable to determine current swparams for playback: {}\n", snd_strerror(err));
        return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(m_pHandle, swparams, (m_bufferSize / m_periodSize) * m_periodSize);
    if (err < 0)
    {
        LogError("Unable to set start threshold mode for playback: {}\n", snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    err = snd_pcm_sw_params_set_avail_min(m_pHandle, swparams, periodEvent ? m_bufferSize : m_periodSize);
    if (err < 0)
    {
        LogError("Unable to set avail min for playback: {}\n", snd_strerror(err));
        return err;
    }
    /* enable period events when requested */
    if (periodEvent)
    {
        err = snd_pcm_sw_params_set_period_event(m_pHandle, swparams, 1);
        if (err < 0)
        {
            LogError("Unable to set period event: {}\n", snd_strerror(err));
            return err;
        }
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(m_pHandle, swparams);
    if (err < 0)
    {
        LogError("Unable to set sw params for playback: {}\n", snd_strerror(err));
        return err;
    }
    return 0;
}

Mixer&
Mixer::init()
{
    LogInfo("initializing alsa...\n");

    openAlsa();

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

    snd_pcm_hw_params_free(m_pHwParams);
    snd_pcm_sw_params_free(m_pSwParams);

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

    LogInfo("bPause: {}\n", bPause);
    m_atom_bPaused.store(bPause, atomic::ORDER::RELEASE);

    LockScope lock {&m_mtxLoop};

    if (bPause)
    {
    }
    else
    {
        m_cndLoop.signal();
    }

    mpris::playbackStatusChanged();
}

void
Mixer::changeSampleRate(u64 sampleRate, bool bSave)
{
    int err = 0;

    pause(true);

    err = snd_pcm_set_params(
        m_pHandle,
        SND_PCM_FORMAT_FLOAT,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        m_nChannels, sampleRate, 1, 50000
    );

    pause(false);

    if (err < 0)
    {
        LogError{"{}\n", snd_strerror(err)};
        return;
    }

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

void
Mixer::openAlsa()
{
    int err = 0;

    ADT_RUNTIME_EXCEPTION_FMT(
        (err = snd_pcm_open(&m_pHandle, ntsDEVICE, SND_PCM_STREAM_PLAYBACK, 0)) >= 0,
        "({}): {}", err, snd_strerror(err)
    );

    snd_pcm_hw_params_malloc(&m_pHwParams);
    snd_pcm_sw_params_malloc(&m_pSwParams);

    snd_lib_error_set_handler(errorHandler);

    ADT_RUNTIME_EXCEPTION_FMT((err = setHwParams(m_pHwParams, SND_PCM_ACCESS_RW_INTERLEAVED)) >= 0, "{}", snd_strerror(err));
    ADT_RUNTIME_EXCEPTION_FMT((err = setSwParams(m_pSwParams)) >= 0, "{}", snd_strerror(err));
}

void
Mixer::xrunRecovery()
{
    LogWarn{"reinitializing alsa...\n"};
    snd_pcm_drop(m_pHandle);
    snd_pcm_close(m_pHandle);
    snd_pcm_hw_params_free(m_pHwParams);
    snd_pcm_sw_params_free(m_pSwParams);

    openAlsa();
}

THREAD_STATUS
Mixer::loop()
{
    defer( m_atom_bLoopDone.store(true, atomic::ORDER::RELEASE) );

    snd_pcm_start(m_pHandle);

    bool bPaused = true;
    bool bReinit = false;

    while (m_atom_bRunning.load(atomic::ORDER::ACQUIRE))
    {
        isize nSamplesRequested = 0;
        const f32 vol = m_bMuted ? 0.0f : std::pow(m_volume * (1.0f/100.0f), 3.0f);

        nSamplesRequested = m_periodSize * m_nChannels;
        m_ringBuff.pop({audio::g_aDrainBuffer, nSamplesRequested});

        for (isize sampleI = 0; sampleI < nSamplesRequested; ++sampleI)
            audio::g_aDrainBuffer[sampleI] *= vol;

        {
            LockScope lock {&m_mtxLoop};

            while (m_atom_bPaused.load(atomic::ORDER::ACQUIRE))
            {
                bPaused = true;
                snd_pcm_drop(m_pHandle);
                m_cndLoop.wait(&m_mtxLoop);

                if (!m_atom_bRunning.load(atomic::ORDER::ACQUIRE))
                    goto GOTO_done;
            }

            if (bPaused)
            {
                bPaused = false;
                bReinit = false;
                snd_pcm_prepare(m_pHandle);
            }

            if (bReinit)
            {
                bReinit = false;
                xrunRecovery();
            }

            f32* ptr = audio::g_aDrainBuffer;
            isize cptr = m_periodSize;
            while (cptr > 0)
            {
                snd_pcm_sframes_t err = snd_pcm_writei(m_pHandle, ptr, cptr);
                if (err == -EAGAIN) continue;

                if (err < 0)
                {
                    bReinit = true;
                    break;
                }

                ptr += err * m_nChannels;
                cptr -= err;
            }
        }
    }

GOTO_done:
    LogDebug("LOOP DONE\n");

    return THREAD_STATUS(0);
}

} /* namespace platform::alsa */

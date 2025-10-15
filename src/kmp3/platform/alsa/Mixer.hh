#pragma once

#include "audio.hh"

#include <alsa/asoundlib.h>

namespace platform::alsa
{

struct Mixer : public audio::IMixer
{
    snd_pcm_t *m_pHandle {};

    atomic::Int m_atom_bRunning {false};
    atomic::Int m_atom_bLoopDone {true};
    Thread m_thrdLoop {};
    Mutex m_mtxLoop {INIT};
    CndVar m_cndLoop {INIT};

    unsigned int m_bufferTime = 500000; /* ring buffer length in us */
    unsigned int m_periodTime = 100000; /* period time in us */

    snd_pcm_sframes_t m_bufferSize;
    snd_pcm_sframes_t m_periodSize;

    snd_pcm_hw_params_t *m_pHwParams;
    snd_pcm_sw_params_t *m_pSwParams;

    /* */

    virtual Mixer& init() override;
    virtual void deinit() override;
    virtual bool play(StringView svPath) override;
    virtual void pause(bool bPause) override;
    virtual void changeSampleRate(u64 sampleRate, bool bSave) override;

    void setConfig(u64 sampleRate, int nChannels, bool bSaveNewConfig);
    THREAD_STATUS loop();

protected:
    int setHwParams(snd_pcm_hw_params_t* params, snd_pcm_access_t access);
    int setSwParams(snd_pcm_sw_params_t* swparams);

    void openAlsa();
    void xrunRecovery();
};

} /* namespace platform::alsa */

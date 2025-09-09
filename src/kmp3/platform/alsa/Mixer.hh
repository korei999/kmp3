#pragma once

#include "audio.hh"

#include <alsa/asoundlib.h>

namespace platform::alsa
{

struct Mixer : public audio::IMixer
{
    snd_pcm_t *m_pHandle {};

    adt::f64 m_currMs {};
    adt::atomic::Int m_atom_bRunning {false};
    adt::atomic::Int m_atom_bLoopDone {false};
    adt::Thread m_thrdLoop {};
    adt::Mutex m_mtxLoop {adt::INIT};
    adt::CndVar m_cndLoop {adt::INIT};

    /* */

    virtual Mixer& init() override;
    virtual void destroy() override;
    virtual bool play(adt::StringView svPath) override;
    virtual void pause(bool bPause) override;
    virtual void togglePause() override;
    virtual void changeSampleRate(adt::u64 sampleRate, bool bSave) override;
    virtual void setVolume(const adt::f32 volume) override;
    [[nodiscard]] virtual adt::i64 getCurrentMS() override;
    [[nodiscard]] virtual adt::i64 getTotalMS() override;

    void setConfig(adt::u64 sampleRate, int nChannels, bool bSaveNewConfig);
    adt::THREAD_STATUS loop();
};

} /* namespace platform::alsa */

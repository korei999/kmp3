#pragma once

#include "audio.hh"

#include <sndio.h>

namespace platform::sndio
{

struct Mixer : public audio::IMixer
{
    sio_par m_par {};
    sio_hdl* m_pHdl {};

    atomic::Int m_atom_bRunning {false};
    atomic::Int m_atom_bLoopDone {false};
    Thread m_thrdLoop {};
    Mutex m_mtxLoop {INIT};
    CndVar m_cndLoop {INIT};

    /* */

    virtual Mixer& init() override;
    virtual void deinit() override;
    virtual bool play(StringView svPath) override;
    virtual void pause(bool bPause) override;
    virtual void changeSampleRate(u64 sampleRate, bool bSave) override;

    /* */

    void setConfig(u64 sampleRate, int nChannels, bool bSaveNewConfig);

    THREAD_STATUS loop();

};

} /* namespace platform::sndio */

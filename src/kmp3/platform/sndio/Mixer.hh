#pragma once

#include "audio.hh"

#include <sndio.h>

namespace platform::sndio
{

struct Mixer : public audio::IMixer
{
    sio_par m_par {};
    sio_hdl* m_pHdl {};

    adt::atomic::Int m_atom_bRunning {false};
    adt::atomic::Int m_atom_bLoopDone {false};
    adt::Thread m_thrdLoop {};
    adt::Mutex m_mtxLoop {adt::INIT};
    adt::CndVar m_cndLoop {adt::INIT};

    /* */

    virtual Mixer& init() override;
    virtual void deinit() override;
    virtual bool play(adt::StringView svPath) override;
    virtual void pause(bool bPause) override;
    virtual void togglePause() override;
    virtual void changeSampleRate(adt::u64 sampleRate, bool bSave) override;

    /* */

    void setConfig(adt::u64 sampleRate, int nChannels, bool bSaveNewConfig);

    adt::THREAD_STATUS loop();

};

} /* namespace platform::sndio */

#pragma once

#include "audio.hh"

#include <AudioToolbox/AudioToolbox.h>

namespace platform::apple
{

struct Mixer : public audio::IMixer
{
    AudioUnit m_unit {};
    adt::f64 m_currMs {};

    /* */

    virtual Mixer& init() override;
    virtual void destroy() override;
    virtual bool play(adt::StringView svPath) override;
    virtual void pause(bool bPause) override;
    virtual void togglePause() override;
    virtual void changeSampleRate(adt::u64 sampleRate, bool bSave) override;
    virtual void seekMS(adt::f64 ms) override;
    virtual void seekOff(adt::f64 offset) override;
    virtual void setVolume(const adt::f32 volume) override;
    [[nodiscard]] virtual adt::i64 getCurrentMS() override;
    [[nodiscard]] virtual adt::i64 getTotalMS() override;

    /* */

    void setConfig(adt::f64 sampleRate, int nChannels, bool bSaveNewConfig);

    OSStatus writeCallBack(
        AudioUnitRenderActionFlags* pIOActionFlags,
        const AudioTimeStamp* pInTimeStamp,
        adt::u32 inBusNumber,
        adt::u32 inNumberFrames,
        AudioBufferList* pIOData
    );
};

} /* namespace platform::apple */

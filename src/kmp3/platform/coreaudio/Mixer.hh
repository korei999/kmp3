#pragma once

#include "audio.hh"

#include <AudioToolbox/AudioToolbox.h>

namespace platform::coreaudio
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

protected:
    /* Apparently locking mutex in the callback is a bad idea, tho it seems to be working just fine. https://lists.apple.com/archives/coreaudio-api/2009/May/msg00031.html?utm_source=chatgpt.com */
    void writeFramesNonLocked(adt::Span<adt::f32> spBuff, adt::u32 nFrames, long* pSamplesWritten, adt::i64* pPcmPos);
};

} /* namespace platform::apple */

#pragma once

#include "audio.hh"

#include <AudioToolbox/AudioToolbox.h>

namespace platform::coreaudio
{

struct Mixer : public audio::IMixer
{
    AudioUnit m_unit {};

    /* */

    virtual Mixer& init() override;
    virtual void deinit() override;
    virtual bool play(StringView svPath) override;
    virtual void pause(bool bPause) override;
    virtual void changeSampleRate(u64 sampleRate, bool bSave) override;

    /* */

    void setConfig(f64 sampleRate, int nChannels, bool bSaveNewConfig);

    OSStatus writeCallBack(
        AudioUnitRenderActionFlags* pIOActionFlags,
        const AudioTimeStamp* pInTimeStamp,
        u32 inBusNumber,
        u32 inNumberFrames,
        AudioBufferList* pIOData
    );

protected:
    /* Apparently locking mutex in the callback is a bad idea, tho it seems to be working just fine. https://lists.apple.com/archives/coreaudio-api/2009/May/msg00031.html?utm_source=chatgpt.com */
    void writeFramesNonLocked(Span<f32> spBuff, u32 nFrames, long* pSamplesWritten, i64* pPcmPos);
};

} /* namespace platform::apple */

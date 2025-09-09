#include "Mixer.hh"

#include "app.hh"

using namespace adt;

namespace platform::coreaudio
{

void
Mixer::setConfig(adt::f64 sampleRate, int nChannels, bool bSaveNewConfig)
{
    sampleRate = utils::clamp(sampleRate, f64(app::g_config.minSampleRate), f64(app::g_config.maxSampleRate));

    if (bSaveNewConfig)
    {
        m_changedSampleRate = m_sampleRate = sampleRate;
        m_nChannels = nChannels;
    }

    AudioStreamBasicDescription desk {};
    desk.mSampleRate = sampleRate;
    desk.mFormatID = kAudioFormatLinearPCM;
    desk.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    desk.mFramesPerPacket = 1;
    desk.mChannelsPerFrame = nChannels;
    desk.mBitsPerChannel = 32;
    desk.mBytesPerFrame = (desk.mBitsPerChannel / 8) * desk.mChannelsPerFrame;
    desk.mBytesPerPacket = desk.mBytesPerFrame * desk.mFramesPerPacket;

    AudioUnitSetProperty(
        m_unit,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Input,
        0,
        &desk,
        sizeof(desk)
    );
}

OSStatus
Mixer::writeCallBack(
    [[maybe_unused]] AudioUnitRenderActionFlags* pIOActionFlags,
    [[maybe_unused]] const AudioTimeStamp* pInTimeStamp,
    [[maybe_unused]] u32 inBusNumber,
    u32 inNumberFrames,
    AudioBufferList* pIOData
)
{
    f32 *pDest = static_cast<f32*>(pIOData->mBuffers[0].mData);

    const f32 vol = m_bMuted ? 0.0f : std::pow(m_volume, 3.0f);

    const isize nDecodedSamples = inNumberFrames * m_nChannels;
    isize nWrites = 0;
    isize destI = 0;

    m_ringBuff.pop({audio::g_aRenderBuffer, nDecodedSamples});
    m_currMs = app::decoder().getCurrentMS();

    for (isize i = 0; i < nDecodedSamples; ++i)
        pDest[destI++] = audio::g_aRenderBuffer[nWrites++] * vol;

    if (nDecodedSamples == 0)
    {
        m_currentTimeStamp = 0;
        m_nTotalSamples = 0;
    }
    else
    {
        m_nTotalSamples = app::decoder().getTotalSamplesCount();
    }

    return noErr;
}

Mixer&
Mixer::init()
{
    LogDebug("initializing coreaudio...\n");

    AudioComponentDescription desc {};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    AudioComponentInstanceNew(comp, &m_unit);

    AudioStreamBasicDescription streamFormat {};
    streamFormat.mSampleRate = m_sampleRate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mChannelsPerFrame = m_nChannels;
    streamFormat.mBitsPerChannel = 32;
    streamFormat.mBytesPerFrame = (streamFormat.mBitsPerChannel / 8) * streamFormat.mChannelsPerFrame;
    streamFormat.mBytesPerPacket = streamFormat.mBytesPerFrame * streamFormat.mFramesPerPacket;

    AudioUnitSetProperty(
        m_unit,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Input,
        0,
        &streamFormat,
        sizeof(streamFormat)
    );

    AURenderCallbackStruct callbackStruct {};
    callbackStruct.inputProc = decltype(AURenderCallbackStruct::inputProc)(methodPointerNonVirtual(&Mixer::writeCallBack));
    callbackStruct.inputProcRefCon = this;

    AudioUnitSetProperty(
        m_unit,
        kAudioUnitProperty_SetRenderCallback,
        kAudioUnitScope_Input,
        0,
        &callbackStruct,
        sizeof(callbackStruct)
    );

    AudioUnitInitialize(m_unit);
    AudioOutputUnitStart(m_unit);

    return *this;
}

void
Mixer::destroy()
{
    AudioOutputUnitStop(m_unit);
    AudioUnitUninitialize(m_unit);
    AudioComponentInstanceDispose(m_unit);
}

bool
Mixer::play(StringView svPath)
{
    const f64 prevSpeed = f64(m_changedSampleRate) / f64(m_sampleRate);

    pause(true);

    if (!play2(svPath)) return false;

    setConfig(app::decoder().getSampleRate(), app::decoder().getChannelsCount(), true);

    if (!math::eq(prevSpeed, 1.0))
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

    if (bPause) AudioOutputUnitStop(m_unit);
    else AudioOutputUnitStart(m_unit);
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
Mixer::setVolume(const f32 volume)
{
    m_volume = utils::clamp(volume, 0.0f, app::g_config.maxVolume);
}

i64
Mixer::getCurrentMS()
{
    return m_currMs;
}

i64
Mixer::getTotalMS()
{
    return app::decoder().getTotalMS();
}

} /* namespace platform::apple */

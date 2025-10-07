#include "Mixer.hh"

#include "app.hh"

#include <cmath>

namespace platform::coreaudio
{

void
Mixer::setConfig(f64 sampleRate, int nChannels, bool bSaveNewConfig)
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
    const f32 vol = m_bMuted ? 0.0f : std::pow(m_volume * (1.f/100.f), 3.0f);
    const isize nSamplesRequested = inNumberFrames * m_nChannels;
    m_ringBuff.pop({pDest, nSamplesRequested});

    for (isize sampleI = 0; sampleI < nSamplesRequested; ++sampleI)
        pDest[sampleI] *= vol;

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
    m_atom_bPaused.store(true, atomic::ORDER::RELAXED);

    return *this;
}

void
Mixer::deinit()
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

    m_atom_bPaused.store(bPause, atomic::ORDER::RELEASE);

    if (bPause) AudioOutputUnitStop(m_unit);
    else AudioOutputUnitStart(m_unit);
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

} /* namespace platform::apple */

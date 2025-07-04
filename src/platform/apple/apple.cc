#include "apple.hh"

#include "app.hh"

#include "adt/logs.hh"
#include "adt/math.hh"

using namespace adt;

namespace platform::apple
{

void
Mixer::setConfig(adt::f64 sampleRate, int nChannels, bool bSaveNewConfig)
{
    sampleRate = utils::clamp(sampleRate, f64(defaults::MIN_SAMPLE_RATE), f64(defaults::MAX_SAMPLE_RATE));

    if (bSaveNewConfig)
    {
        m_changedSampleRate = m_sampleRate = sampleRate;
        m_nChannels = nChannels;
    }

    AudioStreamBasicDescription streamFormat {};
    streamFormat.mSampleRate = sampleRate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mChannelsPerFrame = nChannels;
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

    static long s_nDecodedSamples = 0;
    static long s_nWrites = 0;

    const f32 vol = m_bMuted ? 0.0f : std::pow(m_volume, 3.0f);

    isize destI = 0;
    for (u32 i = 0; i < inNumberFrames; ++i)
    {
        /* fill the buffer when it's empty */
        if (s_nWrites >= s_nDecodedSamples)
        {
            writeFramesLocked({audio::g_aRenderBuffer}, inNumberFrames, &s_nDecodedSamples, &m_currentTimeStamp);

            m_currMs = app::decoder().getCurrentMS();
            s_nWrites = 0;
        }

        for (u32 chIdx = 0; chIdx < m_nChannels; ++chIdx)
        {
            /* modify each sample here */
            pDest[destI++] = audio::g_aRenderBuffer[s_nWrites++] * vol;
        }
    }

    if (s_nDecodedSamples == 0)
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
    LOG_NOTIFY("initializing coreaudio...\n");

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

    {
        LockGuard lockDec {&app::decoder().m_mtx};

        if (m_atom_bDecodes.load(atomic::ORDER::ACQUIRE)) app::decoder().close();

        if (audio::ERROR err = app::decoder().open(svPath);
            err != audio::ERROR::OK_
        )
        {
            return false;
        }

        m_atom_bDecodes.store(true, atomic::ORDER::RELAXED);
    }

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
Mixer::seekMS(f64 ms)
{
    LockGuard lock {&app::decoder().m_mtx};

    if (!m_atom_bDecodes.load(atomic::ORDER::ACQUIRE)) return;

    ms = utils::clamp(ms, 0.0, f64(app::decoder().getTotalMS()));
    app::decoder().seekMS(ms);

    m_currMs = ms;
    m_currentTimeStamp = (ms * m_sampleRate * m_nChannels) / 1000.0;
    m_nTotalSamples = app::decoder().getTotalSamplesCount();
}

void
Mixer::seekOff(f64 offset)
{
    auto time = app::decoder().getCurrentMS() + offset;
    seekMS(time);
}

void
Mixer::setVolume(const f32 volume)
{
    m_volume = utils::clamp(volume, 0.0f, defaults::MAX_VOLUME);
}

i64
Mixer::getCurrentMS()
{
    return m_currMs;
}

i64
Mixer::getTotalMS()
{
    LockGuard lock {&app::decoder().m_mtx};
    return app::decoder().getTotalMS();
}

} /* namespace platform::apple */

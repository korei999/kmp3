#include "audio.hh"

#include "app.hh"

#include "adt/Thread.hh"

using namespace adt;

namespace audio
{

f32 g_aRenderBuffer[CHUNK_SIZE] {};

void
IMixer::writeFramesLocked(Span<f32> spBuff, u32 nFrames, long* pSamplesWritten, i64* pPcmPos)
{
    audio::ERROR err {};
    {
        LockGuard lockDec {&app::decoder().m_mtx};

        if (!m_atom_bDecodes.load(atomic::ORDER::ACQUIRE)) return;

        err = app::decoder().writeToBuffer(
            spBuff,
            nFrames, m_nChannels,
            pSamplesWritten, pPcmPos
        );

        if (err == audio::ERROR::END_OF_FILE)
        {
            pause(true);
            app::decoder().close();
            m_atom_bDecodes.store(false, atomic::ORDER::RELEASE);
        }
    }

    if (err == audio::ERROR::END_OF_FILE)
        m_atom_bSongEnd.store(true, atomic::ORDER::RELEASE);
}

bool
IMixer::isMuted() const
{
    return m_bMuted;
}

void
IMixer::toggleMute()
{
    utils::toggle(&m_bMuted);
}

u32
IMixer::getSampleRate() const
{
    return m_sampleRate;
}

u32
IMixer::getChangedSampleRate() const
{
    return m_changedSampleRate;
}

u8
IMixer::getNChannels() const
{
    return m_nChannels;
}

u64
IMixer::getTotalSamplesCount() const
{
    return m_nTotalSamples;
}

u64
IMixer::getCurrentTimeStamp() const
{
    return m_currentTimeStamp;
}

const atomic::Int&
IMixer::isPaused() const
{
    return m_atom_bPaused;
}

f64
IMixer::getVolume() const
{
    return m_volume;
}

void
IMixer::volumeDown(const f32 step)
{
    setVolume(m_volume - step);
}

void
IMixer::volumeUp(const f32 step)
{
    setVolume(m_volume + step);
}

f64
IMixer::calcCurrentMS()
{
    return (f64(m_currentTimeStamp) / f64(m_sampleRate) / f64(m_nChannels)) * 1000.0;
}

f64
IMixer::calcTotalMS()
{
    return (f64(m_nTotalSamples) / f64(m_sampleRate) / f64(m_nChannels)) * 1000.0;
}

void
IMixer::changeSampleRateDown(int ms, bool bSave)
{
    changeSampleRate(m_changedSampleRate - ms, bSave);
}

void
IMixer::changeSampleRateUp(int ms, bool bSave)
{
    changeSampleRate(m_changedSampleRate + ms, bSave);
}

void
IMixer::restoreSampleRate()
{
    changeSampleRate(m_sampleRate, false);
}

} /* namespace audio */

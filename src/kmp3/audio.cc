#include "audio.hh"

#include "app.hh"

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

RingBuffer::RingBuffer(isize capacityPowOf2)
    : m_cap{nextPowerOf2(capacityPowOf2)},
      m_pData{StdAllocator::inst()->zallocV<f32>(m_cap)},
      m_mtx{Mutex::TYPE::PLAIN}
{
    ADT_ASSERT(capacityPowOf2 > 0, "capacityPowOf2: {}", capacityPowOf2);
}

void
RingBuffer::destroy() noexcept
{
    {
        LockGuard lockGuard {&m_mtx};
        m_bDone = true;
        m_cnd.signal();
        m_thrd.join();
    }

    m_mtx.destroy();
    m_cnd.destroy();
    StdAllocator::inst()->free(m_pData);

    *this = {};
}

bool
RingBuffer::push(const Span<const f32> sp) noexcept
{
    LockGuard lockGuard {&m_mtx};

    if (sp.size() + m_size > m_cap)
    {
        LogWarn{"dropping out of range push (size: {}\n", sp.size()};
        return false;
    }

    const isize nextLastI = (m_lastI + sp.size()) & (m_cap - 1);

    if (m_firstI <= m_lastI)
    {
        const isize nUntilEnd = m_cap - m_lastI;
        ::memcpy(m_pData + m_lastI, sp.data(), nUntilEnd);
        ::memcpy(m_pData, sp.data(), sp.size() - nUntilEnd);
    }
    else
    {
        ::memcpy(m_pData + m_lastI, sp.data(), sp.size());
    }

    m_lastI = nextLastI;
    m_size += sp.size();
    return true;
}

isize
RingBuffer::pop(Span<f32> sp) noexcept
{
    LockGuard lockGuard {&m_mtx};

    /* Write as much as we can. */
    if (sp.size() > m_size) sp.m_size = m_size;

    const isize nextFirstI = (m_firstI + sp.size()) & (m_cap - 1);

    if (m_firstI >= m_lastI)
    {
        const isize nUntilEnd = m_cap - m_firstI;
        ::memcpy(sp.data(), m_pData + m_firstI, nUntilEnd);
        ::memcpy(sp.data() + nUntilEnd, m_pData, sp.size() - nUntilEnd);
    }
    else
    {
        ::memcpy(sp.data(), m_pData, sp.size());
    }

    m_firstI = nextFirstI;
    m_size -= sp.size();
    return sp.size();
}

} /* namespace audio */

#include "audio.hh"

#include "app.hh"
#include "platform/mpris/mpris.hh"

using namespace adt;

namespace audio
{

f32 g_aDrainBuffer[DRAIN_BUFFER_SIZE] {};

IMixer&
IMixer::startDecoderThread()
{
    new(&m_ringBuff) RingBuffer {RING_BUFFER_SIZE};
    new(&m_ringBuff.m_thrd) Thread {(ThreadFn)methodPointerNonVirtual(&IMixer::refillRingBufferLoop), this};

    return *this;
}

THREAD_STATUS
IMixer::refillRingBufferLoop()
{
    defer( m_ringBuff.m_atom_bLoopDone.store(true, atomic::ORDER::RELAXED) );

    while (m_bRunning)
    {
        {
            LockScope lockRing {&m_ringBuff.m_mtx};
            while (!m_ringBuff.m_bQuit && m_ringBuff.m_size >= RING_BUFFER_LOW_THRESHOLD)
                m_ringBuff.m_cnd.wait(&m_ringBuff.m_mtx);

            if (m_ringBuff.m_bQuit) break;
        }

        fillRingBuffer();
    }

    return THREAD_STATUS{0};
}

bool
IMixer::playFinal(adt::StringView svPath)
{
    LockScope lockDec {&app::decoder().m_mtx};

    if (m_atom_bDecodes.load(atomic::ORDER::ACQUIRE)) app::decoder().close();

    m_ringBuff.clear();
    m_currentTimeStamp = m_nTotalSamples = m_currMs = 0;

    if (audio::ERROR err = app::decoder().open(svPath);
        err != audio::ERROR::OK_
    )
    {
        return false;
    }

    m_nTotalSamples = app::decoder().getTotalSamplesCount();
    m_atom_bDecodes.store(true, atomic::ORDER::RELAXED);
    m_ringBuff.m_cnd.signal();

    return true;
}

IMixer&
IMixer::start()
{
    startDecoderThread().init();
    return *this;
}

void
IMixer::destroy() noexcept
{
    m_bRunning = false;
    {
        LockScope lockRing {&m_ringBuff.m_mtx};
        m_ringBuff.m_bQuit = true;
        m_ringBuff.m_cnd.signal();
    }
    deinit();
    m_ringBuff.destroy();
}

void
IMixer::fillRingBuffer()
{
    LockScope lockDec {&app::decoder().m_mtx};

    if (!m_atom_bDecodes.load(atomic::ORDER::ACQUIRE)) return;

    long samplesWritten = 0;
    audio::ERROR err = app::decoder().writeToRingBuffer(
        &m_ringBuff,
        m_nChannels,
        m_ePcmType,
        &samplesWritten,
        &m_currentTimeStamp
    );
    m_currMs = app::decoder().getCurrentMS();

    if (err == audio::ERROR::END_OF_FILE)
    {
        pause(true);
        app::decoder().close();
        m_ringBuff.clear();
        m_atom_bDecodes.store(false, atomic::ORDER::RELEASE);
        m_atom_bSongEnd.store(true, atomic::ORDER::RELEASE);
    }
}

bool
IMixer::isMuted() const
{
    return m_bMuted;
}

void
IMixer::toggleMute()
{
    m_bMuted = !m_bMuted;
}

void
IMixer::togglePause()
{
    pause(!m_atom_bPaused.load(atomic::ORDER::ACQUIRE));
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

const atomic::Bool&
IMixer::isPaused() const
{
    return m_atom_bPaused;
}

int
IMixer::getVolume() const
{
    return m_volume;
}

void
IMixer::volumeDown(const int step)
{
    setVolume(m_volume - step);
}

void
IMixer::volumeUp(const int step)
{
    setVolume(m_volume + step);
}

void
IMixer::setVolume(const int volume)
{
    m_volume = utils::clamp(volume, 0, app::g_config.maxVolume);
    LogInfo{"volume: {}\n", m_volume};
#ifdef OPT_MPRIS
    mpris::volumeChanged();
#endif
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

void
IMixer::seekMS(adt::f64 ms)
{
    {
        LockScope lock {&app::decoder().m_mtx};

        if (!m_atom_bDecodes.load(atomic::ORDER::ACQUIRE)) return;

        ms = utils::clamp(ms, 0.0, f64(app::decoder().getTotalMS()));
        m_ringBuff.clear();
        m_ringBuff.m_cnd.signal();
        app::decoder().seekMS(ms);

        m_currMs = ms;
        m_currentTimeStamp = (ms * m_sampleRate * m_nChannels) / 1000.0;
        m_nTotalSamples = app::decoder().getTotalSamplesCount();
    }

    mpris::seeked();
}

void
IMixer::seekOff(adt::f64 offset)
{
    f64 time = app::decoder().getCurrentMS() + offset;
    seekMS(time);
}

i64
IMixer::getCurrentMS()
{
    return m_currMs;
}

i64
IMixer::getTotalMS()
{
    LockScope lock {&app::decoder().m_mtx};
    return app::decoder().getTotalMS();
}

RingBuffer::RingBuffer(isize capacityPowOf2)
    : m_cap{nextPowerOf2(capacityPowOf2)},
      m_pData{StdAllocator::inst()->zallocV<f32>(m_cap)},
      m_mtx{Mutex::TYPE::PLAIN},
      m_cnd{INIT}
{
    ADT_ASSERT(capacityPowOf2 > 0, "capacityPowOf2: {}", capacityPowOf2);
}

void
RingBuffer::destroy() noexcept
{
    while (!m_atom_bLoopDone.load(atomic::ORDER::RELAXED))
        m_cnd.signal();

    m_thrd.join();

    m_mtx.destroy();
    m_cnd.destroy();
    StdAllocator::inst()->free(m_pData);

    *this = {};
}

isize
RingBuffer::push(const Span<const f32> sp) noexcept
{
    LockScope lock {&m_mtx};

    if (sp.size() + m_size > m_cap)
    {
        LogWarn{"dropping out of range push (sp.size: {}, size, cap)\n", sp.size(), m_size, m_cap};
        return sp.size();
    }

    const isize nextLastI = (m_lastI + sp.size()) & (m_cap - 1);

    if (m_lastI >= m_firstI)
    {
        const isize nUntilEnd = utils::min(m_cap - m_lastI, sp.size());
        utils::memCopy(m_pData + m_lastI, sp.data(), nUntilEnd);
        utils::memCopy(m_pData, sp.data() + nUntilEnd, sp.size() - nUntilEnd);
    }
    else
    {
        utils::memCopy(m_pData + m_lastI, sp.data(), sp.size());
    }

    m_lastI = nextLastI;
    m_size += sp.size();
    m_cnd.signal();
    return m_size;
}

isize
RingBuffer::pop(Span<f32> sp) noexcept
{
    LockScope lock {&m_mtx};

    if (sp.size() > m_cap)
    {
        LogWarn{"dropping some size: {} to {}\n", sp.size(), m_cap};
        sp.m_size = m_cap;
    }

    while (!m_bQuit && sp.size() > m_size)
    {
        LogWarn{"waiting for: {}, (m_size: {})\n", sp.size(), m_size};
        m_cnd.wait(&m_mtx);
    }
    if (m_bQuit) return 0;

    const isize nextFirstI = (m_firstI + sp.size()) & (m_cap - 1);

    if (m_firstI >= m_lastI)
    {
        const isize nUntilEnd = utils::min(m_cap - m_firstI, sp.size());
        utils::memCopy(sp.data(), m_pData + m_firstI, nUntilEnd);
        utils::memCopy(sp.data() + nUntilEnd, m_pData, sp.size() - nUntilEnd);
    }
    else
    {
        utils::memCopy(sp.data(), m_pData + m_firstI, sp.size());
    }

    m_firstI = nextFirstI;
    m_size -= sp.size();
    if (m_size < RING_BUFFER_LOW_THRESHOLD) m_cnd.signal();
    return m_size;
}

void
RingBuffer::clear() noexcept
{
    LockScope lock {&m_mtx};
    m_firstI = m_lastI = m_size = 0;
}

} /* namespace audio */

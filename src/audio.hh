#pragma once

#include "Image.hh"

#include "adt/String.hh"
#include "adt/atomic.hh"
#include "adt/utils.hh"

using namespace adt;

namespace audio
{

constexpr u64 CHUNK_SIZE = (1 << 18); /* big enough */
/* Platrform abstracted audio interface */

class IMixer
{
protected:
    atomic::Int m_bPaused {false};
#ifdef OPT_MPRIS
    atomic::Int m_bUpdateMpris {false};
#endif
    bool m_bMuted = false;
    bool m_bRunning = true;
    u32 m_sampleRate = 48000;
    u32 m_changedSampleRate = 48000;
    u8 m_nChannels = 2;
    f32 m_volume = 0.5f;
    i64 m_currentTimeStamp {};
    i64 m_nTotalSamples {};

public:
    virtual void init() = 0;
    virtual void destroy() = 0;
    virtual void play(StringView sPath) = 0;
    virtual void pause(bool bPause) = 0;
    virtual void togglePause() = 0;
    virtual void changeSampleRate(u64 sampleRate, bool bSave) = 0;
    virtual void seekMS(f64 ms) = 0;
    virtual void seekOff(f64 offset) = 0;
    [[nodiscard]] virtual StringView getMetadata(const StringView sKey) = 0;
    [[nodiscard]] virtual Image getCoverImage() = 0;
    virtual void setVolume(const f32 volume) = 0;
    [[nodiscard]] virtual i64 getCurrentMS() = 0;
    [[nodiscard]] virtual i64 getTotalMS() = 0;

    /* */

    bool isMuted() const { return m_bMuted; }
    void toggleMute() { utils::toggle(&m_bMuted); }
    u32 getSampleRate() const { return m_sampleRate; }
    u32 getChangedSampleRate() const { return m_changedSampleRate; }
    u8 getNChannels() const { return m_nChannels; }
    u64 getTotalSamplesCount() const { return m_nTotalSamples; }
    u64 getCurrentTimeStamp() const { return m_currentTimeStamp; }
    const atomic::Int& isPaused() const { return m_bPaused; }
    f64 getVolume() const { return m_volume; }
    void volumeDown(const f32 step) { setVolume(m_volume - step); }
    void volumeUp(const f32 step) { setVolume(m_volume + step); }
    [[nodiscard]] f64 calcCurrentMS() { return (f64(m_currentTimeStamp) / f64(m_sampleRate) / f64(m_nChannels)) * 1000.0; }
    [[nodiscard]] f64 calcTotalMS() { return (f64(m_nTotalSamples) / f64(m_sampleRate) / f64(m_nChannels)) * 1000.0; }
    void changeSampleRateDown(int ms, bool bSave) { changeSampleRate(m_changedSampleRate - ms, bSave); }
    void changeSampleRateUp(int ms, bool bSave) { changeSampleRate(m_changedSampleRate + ms, bSave); }
    void restoreSampleRate() { changeSampleRate(m_sampleRate, false); }

#ifdef OPT_MPRIS
    const atomic::Int& mprisHasToUpdate() const { return m_bUpdateMpris; }
    void mprisSetToUpdate(bool b) { m_bUpdateMpris.store(b, atomic::ORDER::RELEASE); }
#endif
};

struct DummyMixer : public IMixer
{
    virtual void init() override final {}
    virtual void destroy() override final {}
    virtual void play(StringView) override final {}
    virtual void pause(bool) override final {}
    virtual void togglePause() override final {}
    virtual void changeSampleRate(u64, bool) override final {}
    virtual void seekMS(f64) override final {}
    virtual void seekOff(f64) override final {}
    virtual StringView getMetadata(const StringView) override final { return {}; }
    virtual Image getCoverImage() override final { return {}; }
    virtual void setVolume(const f32) override final {}
    virtual i64 getCurrentMS() override final { return {}; };
    virtual i64 getTotalMS() override final { return {}; };
};

enum class ERROR : u8
{
    OK_ = 0,
    END_OF_FILE,
    DONE,
    FAIL,
};

constexpr StringView mapERRORToString[] {
    "OK_",
    "EOF_OF_FILE",
    "DONE",
    "FAIL",
};

struct IDecoder
{
    [[nodiscard]] virtual ERROR writeToBuffer(
        Span<f32> spBuff,
        const int nFrames,
        const int nChannles,
        long* pSamplesWritten,
        ssize* pPcmPos
    ) = 0;

    [[nodiscard]] virtual u32 getSampleRate() = 0;
    virtual void seekMS(f64 ms) = 0;
    [[nodiscard]] virtual i64 getCurrentSamplePos() = 0;
    [[nodiscard]] virtual i64 getCurrentMS() = 0;
    [[nodiscard]] virtual i64 getTotalMS() = 0;
    [[nodiscard]] virtual i64 getTotalSamplesCount() = 0;
    [[nodiscard]] virtual int getChannelsCount() = 0;
    [[nodiscard]] virtual StringView getMetadataValue(const StringView sKey) = 0;
    [[nodiscard]] virtual Image getCoverImage() = 0;
    [[nodiscard]] virtual ERROR open(StringView sPath) = 0;
    virtual void close() = 0;
};

} /* namespace audio */

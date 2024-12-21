#pragma once

#include "Image.hh"
#include "adt/Opt.hh"
#include "adt/String.hh"
#include "adt/utils.hh"

#include <atomic>

using namespace adt;

namespace audio
{

constexpr u64 CHUNK_SIZE = (1 << 18); /* big enough */
/* Platrform abstracted audio interface */

class IMixer
{
protected:
    std::atomic<bool> m_bPaused = false;
#ifdef USE_MPRIS
    std::atomic<bool> m_bUpdateMpris {};
#endif
    bool m_bMuted = false;
    bool m_bRunning = true;
    u32 m_sampleRate = 48000;
    u32 m_changedSampleRate = 48000;
    u8 m_nChannels = 2;
    f32 m_volume = 0.5f;
    u64 m_currentTimeStamp {};
    u64 m_totalSamplesCount {};

public:
    virtual void init() = 0;
    virtual void destroy() = 0;
    virtual void play(String sPath) = 0;
    virtual void pause(bool bPause) = 0;
    virtual void togglePause() = 0;
    virtual void changeSampleRate(u64 sampleRate, bool bSave) = 0;
    virtual void seekMS(s64 ms) = 0;
    virtual void seekLeftMS(s64 ms) = 0;
    virtual void seekRightMS(s64 ms) = 0;
    [[nodiscard]] virtual Opt<String> getMetadata(const String sKey) = 0;
    [[nodiscard]] virtual Opt<Image> getCoverImage() = 0;
    virtual void setVolume(const f32 volume) = 0;

    /* */

    bool isMuted() const { return m_bMuted; }
    void toggleMute() { utils::toggle(&m_bMuted); }
    u32 getSampleRate() const { return m_sampleRate; }
    u32 getChangedSampleRate() const { return m_changedSampleRate; }
    u8 getNChannels() const { return m_nChannels; }
    u64 getTotalSamplesCount() const { return m_totalSamplesCount; }
    u64 getCurrentTimeStamp() const { return m_currentTimeStamp; }
    const std::atomic<bool>& isPaused() const { return m_bPaused; }
    f64 getVolume() const { return m_volume; }
    void volumeDown(const f32 step) { setVolume(m_volume - step); }
    void volumeUp(const f32 step) { setVolume(m_volume + step); }
    [[nodiscard]] f64 getCurrentMS() { return (f64(m_currentTimeStamp) / f64(m_sampleRate) / f64(m_nChannels)) * 1000.0; }
    [[nodiscard]] f64 getMaxMS() { return (f64(m_totalSamplesCount) / f64(m_sampleRate) / f64(m_nChannels)) * 1000.0; }
    void changeSampleRateDown(int ms, bool bSave) { changeSampleRate(m_changedSampleRate - ms, bSave); }
    void changeSampleRateUp(int ms, bool bSave) { changeSampleRate(m_changedSampleRate + ms, bSave); }
    void restoreSampleRate() { changeSampleRate(m_sampleRate, false); }

#ifdef USE_MPRIS
    const std::atomic<bool>& mprisHasToUpdate() const { return m_bUpdateMpris; }
    void mprisSetToUpdate(bool b) { m_bUpdateMpris = b; }
#endif
};

struct DummyMixer : public IMixer
{
    virtual void init() override final {}
    virtual void destroy() override final {}
    virtual void play(String) override final {}
    virtual void pause(bool) override final {}
    virtual void togglePause() override final {}
    virtual void changeSampleRate(u64, bool) override final {}
    virtual void seekMS(s64) override final {}
    virtual void seekLeftMS(s64) override final {}
    virtual void seekRightMS(s64) override final {}
    virtual Opt<String> getMetadata(const String) override final { return {}; }
    virtual Opt<Image> getCoverImage() override final { return {}; }
    virtual void setVolume(const f32) override final {}
};

enum class ERROR : u8
{
    OK_ = 0,
    END_OF_FILE,
    DONE,
    FAIL,
};

constexpr String mapERRORToString[] {
    "OK_",
    "EOF_OF_FILE",
    "DONE",
    "FAIL",
};

struct IReader
{
    [[nodiscard]] virtual ERROR writeToBuffer(
        f32* pBuff, const u32 buffSize, const u32 nFrames, const u32 nChannles,
        long* pSamplesWritten, u64* pPcmPos
    ) = 0;

    [[nodiscard]] virtual u32 getSampleRate() = 0;
    virtual void seekMS(long ms) = 0;
    [[nodiscard]] virtual long getCurrentSamplePos() = 0;
    [[nodiscard]] virtual long getTotalSamplesCount() = 0;
    [[nodiscard]] virtual int getChannelsCount() = 0;
    [[nodiscard]] virtual Opt<String> getMetadataValue(const String sKey) = 0;
    [[nodiscard]] virtual Opt<Image> getCoverImage() = 0;
    [[nodiscard]] virtual ERROR open(String sPath) = 0;
    virtual void close() = 0;
};

} /* namespace audio */

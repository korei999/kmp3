#pragma once

#include "Image.hh"

#include "adt/StringDecl.hh"
#include "adt/atomic.hh"
#include "adt/utils.hh"

namespace audio
{

constexpr adt::u64 CHUNK_SIZE = (1 << 18); /* big enough */

/* Platrform abstracted audio interface */
struct IMixer
{
    adt::atomic::Int m_atom_bPaused {false};
#ifdef OPT_MPRIS
    adt::atomic::Int m_atom_bUpdateMpris {false};
#endif
    adt::atomic::Int m_bSongEnd {false};
    bool m_bMuted = false;
    bool m_bRunning = true;
    adt::u32 m_sampleRate = 48000;
    adt::u32 m_changedSampleRate = 48000;
    adt::u8 m_nChannels = 2;
    adt::f32 m_volume = 0.5f;
    adt::i64 m_currentTimeStamp {};
    adt::i64 m_nTotalSamples {};

    virtual void init() = 0;
    virtual void destroy() = 0;
    virtual void play(adt::StringView svPath) = 0;
    virtual void pause(bool bPause) = 0;
    virtual void togglePause() = 0;
    virtual void changeSampleRate(adt::u64 sampleRate, bool bSave) = 0;
    virtual void seekMS(adt::f64 ms) = 0;
    virtual void seekOff(adt::f64 offset) = 0;
    virtual void setVolume(const adt::f32 volume) = 0;
    [[nodiscard]] virtual adt::i64 getCurrentMS() = 0;
    [[nodiscard]] virtual adt::i64 getTotalMS() = 0;

    /* */

    bool isMuted() const { return m_bMuted; }
    void toggleMute() { adt::utils::toggle(&m_bMuted); }
    adt::u32 getSampleRate() const { return m_sampleRate; }
    adt::u32 getChangedSampleRate() const { return m_changedSampleRate; }
    adt::u8 getNChannels() const { return m_nChannels; }
    adt::u64 getTotalSamplesCount() const { return m_nTotalSamples; }
    adt::u64 getCurrentTimeStamp() const { return m_currentTimeStamp; }
    const adt::atomic::Int& isPaused() const { return m_atom_bPaused; }
    adt::f64 getVolume() const { return m_volume; }
    void volumeDown(const adt::f32 step) { setVolume(m_volume - step); }
    void volumeUp(const adt::f32 step) { setVolume(m_volume + step); }
    [[nodiscard]] adt::f64 calcCurrentMS() { return (adt::f64(m_currentTimeStamp) / adt::f64(m_sampleRate) / adt::f64(m_nChannels)) * 1000.0; }
    [[nodiscard]] adt::f64 calcTotalMS() { return (adt::f64(m_nTotalSamples) / adt::f64(m_sampleRate) / adt::f64(m_nChannels)) * 1000.0; }
    void changeSampleRateDown(int ms, bool bSave) { changeSampleRate(m_changedSampleRate - ms, bSave); }
    void changeSampleRateUp(int ms, bool bSave) { changeSampleRate(m_changedSampleRate + ms, bSave); }
    void restoreSampleRate() { changeSampleRate(m_sampleRate, false); }

#ifdef OPT_MPRIS
    const adt::atomic::Int& mprisHasToUpdate() const { return m_atom_bUpdateMpris; }
    void mprisSetToUpdate(bool b) { m_atom_bUpdateMpris.store(b, adt::atomic::ORDER::RELEASE); }
#endif
};

struct DummyMixer : public IMixer
{
    virtual void init() override final {}
    virtual void destroy() override final {}
    virtual void play(adt::StringView) override final {}
    virtual void pause(bool) override final {}
    virtual void togglePause() override final {}
    virtual void changeSampleRate(adt::u64, bool) override final {}
    virtual void seekMS(adt::f64) override final {}
    virtual void seekOff(adt::f64) override final {}
    virtual void setVolume(const adt::f32) override final {}
    virtual adt::i64 getCurrentMS() override final { return {}; };
    virtual adt::i64 getTotalMS() override final { return {}; };
};

enum class ERROR : adt::u8
{
    OK_ = 0,
    END_OF_FILE,
    DONE,
    FAIL,
};

constexpr adt::StringView mapERRORToString[] {
    "OK_",
    "EOF_OF_FILE",
    "DONE",
    "FAIL",
};

struct IDecoder
{
    [[nodiscard]] virtual ERROR writeToBuffer(
        adt::Span<adt::f32> spBuff,
        const int nFrames,
        const int nChannles,
        long* pSamplesWritten,
        adt::isize* pPcmPos
    ) = 0;

    [[nodiscard]] virtual adt::u32 getSampleRate() = 0;
    virtual void seekMS(adt::f64 ms) = 0;
    [[nodiscard]] virtual adt::i64 getCurrentSamplePos() = 0;
    [[nodiscard]] virtual adt::i64 getCurrentMS() = 0;
    [[nodiscard]] virtual adt::i64 getTotalMS() = 0;
    [[nodiscard]] virtual adt::i64 getTotalSamplesCount() = 0;
    [[nodiscard]] virtual int getChannelsCount() = 0;
    [[nodiscard]] virtual adt::StringView getMetadata(const adt::StringView sKey) = 0;
    [[nodiscard]] virtual Image getCoverImage() = 0;
    [[nodiscard]] virtual ERROR open(adt::StringView sPath) = 0;
    virtual void close() = 0;
};

} /* namespace audio */

#pragma once

#include "adt/Opt.hh"
#include "adt/String.hh"

#include <atomic>

#include "ffmpeg.hh"

using namespace adt;

namespace audio
{

constexpr u64 CHUNK_SIZE = (1 << 18); /* big enough */
/* Platrform abstracted audio interface */
struct IMixer;
struct Track;

struct MixerVTable
{
    void (*init)(IMixer* s);
    void (*destroy)(IMixer*);
    void (*play)(IMixer*, String);
    void (*pause)(IMixer* s, bool bPause);
    void (*togglePause)(IMixer* s);
    void (*changeSampleRate)(IMixer* s, u64 sampleRate, bool bSave);
    void (*seekMS)(IMixer* s, s64 ms);
    void (*seekLeftMS)(IMixer* s, s64 ms);
    void (*seekRightMS)(IMixer* s, s64 ms);
    Opt<String> (*getMetadata)(IMixer* s, const String sKey);
    Opt<ffmpeg::Image> (*getCoverImage)(IMixer* s);
    void (*setVolume)(IMixer* s, const f32 volume);
};

template<typename MIXER_T>
constexpr MixerVTable
MixerVTableGenerate()
{
    return MixerVTable {
        .init = decltype(MixerVTable::init)(methodPointer(&MIXER_T::init)),
        .destroy = decltype(MixerVTable::destroy)(methodPointer(&MIXER_T::destroy)),
        .play = decltype(MixerVTable::play)(methodPointer(&MIXER_T::play)),
        .pause = decltype(MixerVTable::pause)(methodPointer(&MIXER_T::pause)),
        .togglePause = decltype(MixerVTable::togglePause)(methodPointer(&MIXER_T::togglePause)),
        .changeSampleRate = decltype(MixerVTable::changeSampleRate)(methodPointer(&MIXER_T::changeSampleRate)),
        .seekMS = decltype(MixerVTable::seekMS)(methodPointer(&MIXER_T::seekMS)),
        .seekLeftMS = decltype(MixerVTable::seekLeftMS)(methodPointer(&MIXER_T::seekLeftMS)),
        .seekRightMS = decltype(MixerVTable::seekRightMS)(methodPointer(&MIXER_T::seekRightMS)),
        .getMetadata = decltype(MixerVTable::getMetadata)(methodPointer(&MIXER_T::getMetadata)),
        .getCoverImage = decltype(MixerVTable::getCoverImage)(methodPointer(&MIXER_T::getCoverImage)),
        .setVolume = decltype(MixerVTable::setVolume)(methodPointer(&MIXER_T::setVolume)),
    };
}

struct IMixer
{
    const MixerVTable* pVTable {};

    /* */

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

    /* */

    ADT_NO_UB void init() { pVTable->init(this); }
    ADT_NO_UB void destroy() { pVTable->destroy(this); }
    ADT_NO_UB void play(String sPath) { pVTable->play(this, sPath); }
    ADT_NO_UB void pause(bool bPause) { pVTable->pause(this, bPause); }
    ADT_NO_UB void togglePause() { pVTable->togglePause(this); }
    ADT_NO_UB void changeSampleRate(u64 sampleRate, bool bSave) { pVTable->changeSampleRate(this, sampleRate, bSave); }
    ADT_NO_UB void seekMS(s64 ms) { pVTable->seekMS(this, ms); }
    ADT_NO_UB void seekLeftMS(s64 ms) { pVTable->seekLeftMS(this, ms); }
    ADT_NO_UB void seekRightMS(s64 ms) { pVTable->seekRightMS(this, ms); }
    [[nodiscard]] ADT_NO_UB Opt<String> getMetadata(const String sKey) { return pVTable->getMetadata(this, sKey); }
    [[nodiscard]] ADT_NO_UB Opt<ffmpeg::Image> getCoverImage() { return pVTable->getCoverImage(this); }
    ADT_NO_UB void setVolume(const f32 volume) { pVTable->setVolume(this, volume); }

    /* */

    void volumeDown(const f32 step) { setVolume(m_volume - step); }
    void volumeUp(const f32 step) { setVolume(m_volume + step); }
    [[nodiscard]] f64 getCurrentMS() { return (f64(m_currentTimeStamp) / f64(m_sampleRate) / f64(m_nChannels)) * 1000.0; }
    [[nodiscard]] f64 getMaxMS() { return (f64(m_totalSamplesCount) / f64(m_sampleRate) / f64(m_nChannels)) * 1000.0; }
    void changeSampleRateDown(int ms, bool bSave) { changeSampleRate(m_changedSampleRate - ms, bSave); }
    void changeSampleRateUp(int ms, bool bSave) { changeSampleRate(m_changedSampleRate + ms, bSave); }
    void restoreSampleRate() { changeSampleRate(m_sampleRate, false); }
};

struct DummyMixer
{
    IMixer super;

    constexpr DummyMixer();

    void init() {}
    void destroy() {}
    void play(String) {}
    void pause(bool) {}
    void togglePause() {}
    void changeSampleRate(u64, bool) {}
    void seekMS(s64) {}
    void seekLeftMS(s64) {}
    void seekRightMS(s64) {}
    Opt<String> getMetadata(const String) { return {}; }
    Opt<ffmpeg::Image> getCoverImage() { return {}; }
    void setVolume(const f32) {}
};

inline const MixerVTable inl_DummyMixerVTable = MixerVTableGenerate<DummyMixer>();

constexpr
DummyMixer::DummyMixer()
    : super {&inl_DummyMixerVTable} {}

} /* namespace audio */

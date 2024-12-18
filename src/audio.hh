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

struct IMixer
{
    const MixerVTable* pVTable {};
    std::atomic<bool> bPaused = false;
#ifdef USE_MPRIS
    std::atomic<bool> bUpdateMpris {};
#endif
    bool bMuted = false;
    bool bRunning = true;
    u32 sampleRate = 48000;
    u32 changedSampleRate = 48000;
    u8 nChannels = 2;
    f32 volume = 0.5f;
    u64 currentTimeStamp {};
    u64 totalSamplesCount {};

    ADT_NO_UB void init() { pVTable->init(this); }
    ADT_NO_UB void destroy() { pVTable->destroy(this); }
    ADT_NO_UB void play(String sPath) { pVTable->play(this, sPath); }
    ADT_NO_UB void pause(bool bPause) { pVTable->pause(this, bPause); }
    ADT_NO_UB void togglePause() { pVTable->togglePause(this); }
    ADT_NO_UB void changeSampleRate(u64 sampleRate, bool bSave) { pVTable->changeSampleRate(this, sampleRate, bSave); }
    ADT_NO_UB void seekMS(u64 ms) { pVTable->seekMS(this, ms); }
    ADT_NO_UB void seekLeftMS(u64 ms) { pVTable->seekLeftMS(this, ms); }
    ADT_NO_UB void seekRightMS(u64 ms) { pVTable->seekRightMS(this, ms); }
    [[nodiscard]] ADT_NO_UB Opt<String> getMetadata(const String sKey) { return pVTable->getMetadata(this, sKey); }
    [[nodiscard]] ADT_NO_UB Opt<ffmpeg::Image> getCoverImage() { return pVTable->getCoverImage(this); }
    ADT_NO_UB void setVolume(const f32 volume) { pVTable->setVolume(this, volume); }

    void volumeDown(const f32 step) { setVolume(volume - step); }
    void volumeUp(const f32 step) { setVolume(volume + step); }
    [[nodiscard]] f64 getCurrentMS() { return (f64(currentTimeStamp) / f64(sampleRate) / f64(nChannels)) * 1000.0; }
    [[nodiscard]] f64 getMaxMS() { return (f64(totalSamplesCount) / f64(sampleRate) / f64(nChannels)) * 1000.0; }
    void changeSampleRateDown(int ms, bool bSave) { changeSampleRate(changedSampleRate - ms, bSave); }
    void changeSampleRateUp(int ms, bool bSave) { changeSampleRate(changedSampleRate + ms, bSave); }
    void restoreSampleRate() { changeSampleRate(sampleRate, false); }
};

struct DummyMixer
{
    IMixer super;

    constexpr DummyMixer();
};

inline const MixerVTable inl_DummyMixerVTable {
    .init = decltype(MixerVTable::init)(+[]{}),
    .destroy = decltype(MixerVTable::destroy)(+[]{}),
    .play = decltype(MixerVTable::play)(+[]{}),
    .pause = decltype(MixerVTable::pause)(+[]{}),
    .togglePause = decltype(MixerVTable::togglePause)(+[]{}),
    .changeSampleRate = decltype(MixerVTable::changeSampleRate)(+[]{}),
    .seekMS = decltype(MixerVTable::seekMS)(+[]{}),
    .seekLeftMS = decltype(MixerVTable::seekMS)(+[]{}),
    .seekRightMS = decltype(MixerVTable::seekMS)(+[]{}),
    .getMetadata = decltype(MixerVTable::getMetadata)(+[]{}),
    .getCoverImage = decltype(MixerVTable::getCoverImage)(+[]{}),
    .setVolume = decltype(MixerVTable::setVolume)(+[]{}),
};

constexpr
DummyMixer::DummyMixer()
    : super(&inl_DummyMixerVTable) {}

} /* namespace audio */

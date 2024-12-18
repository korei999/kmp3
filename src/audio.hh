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
    void (IMixer::* init)();
    void (IMixer::* destroy)();
    void (IMixer::* play)(String);
    void (IMixer::* pause)(bool bPause);
    void (IMixer::* togglePause)();
    void (IMixer::* changeSampleRate)(u64 sampleRate, bool bSave);
    void (IMixer::* seekMS)(s64 ms);
    void (IMixer::* seekLeftMS)(s64 ms);
    void (IMixer::* seekRightMS)(s64 ms);
    Opt<String> (IMixer::* getMetadata)(const String sKey);
    Opt<ffmpeg::Image> (IMixer::* getCoverImage)();
    void (IMixer::* setVolume)(const f32 volume);
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

    ADT_NO_UB void init() { (this->*pVTable->init)(); }
    ADT_NO_UB void destroy() { (this->*pVTable->destroy)(); }
    ADT_NO_UB void play(String sPath) { (this->*pVTable->play)(sPath); }
    ADT_NO_UB void pause(bool bPause) { (this->*pVTable->pause)(bPause); }
    ADT_NO_UB void togglePause() { (this->*pVTable->togglePause)(); }
    ADT_NO_UB void changeSampleRate(u64 sampleRate, bool bSave) { (this->*pVTable->changeSampleRate)(sampleRate, bSave); }
    ADT_NO_UB void seekMS(u64 ms) { (this->*pVTable->seekMS)(ms); }
    ADT_NO_UB void seekLeftMS(u64 ms) { (this->*pVTable->seekLeftMS)(ms); }
    ADT_NO_UB void seekRightMS(u64 ms) { (this->*pVTable->seekRightMS)(ms); }
    [[nodiscard]] ADT_NO_UB Opt<String> getMetadata(const String sKey) { return (this->*pVTable->getMetadata)(sKey); }
    [[nodiscard]] ADT_NO_UB Opt<ffmpeg::Image> getCoverImage() { return (this->*pVTable->getCoverImage)(); }
    ADT_NO_UB void setVolume(const f32 volume) { (this->*pVTable->setVolume)(volume); }

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

    void init() {}
    void destroy() {}
    void play(String sPath) {}
    void pause(bool bPause) {}
    void togglePause() {}
    void changeSampleRate(u64 sampleRate, bool bSave) {}
    void seekMS(u64 ms) {}
    void seekLeftMS(u64 ms) {}
    void seekRightMS(u64 ms) {}
    Opt<String> getMetadata(const String sKey) { return {}; }
    Opt<ffmpeg::Image> getCoverImage() { return {}; }
    void setVolume(const f32 volume) {}
};

inline const MixerVTable inl_DummyMixerVTable {
    .init = decltype(MixerVTable::init)(&DummyMixer::init),
    .destroy = decltype(MixerVTable::destroy)(&DummyMixer::destroy),
    .play = decltype(MixerVTable::play)(&DummyMixer::play),
    .pause = decltype(MixerVTable::pause)(&DummyMixer::pause),
    .togglePause = decltype(MixerVTable::togglePause)(&DummyMixer::togglePause),
    .changeSampleRate = decltype(MixerVTable::changeSampleRate)(&DummyMixer::changeSampleRate),
    .seekMS = decltype(MixerVTable::seekMS)(&DummyMixer::seekMS),
    .seekLeftMS = decltype(MixerVTable::seekMS)(&DummyMixer::seekLeftMS),
    .seekRightMS = decltype(MixerVTable::seekMS)(&DummyMixer::seekRightMS),
    .getMetadata = decltype(MixerVTable::getMetadata)(&DummyMixer::getMetadata),
    .getCoverImage = decltype(MixerVTable::getCoverImage)(&DummyMixer::getCoverImage),
    .setVolume = decltype(MixerVTable::setVolume)(&DummyMixer::setVolume),
};

constexpr
DummyMixer::DummyMixer()
    : super(&inl_DummyMixerVTable) {}

} /* namespace audio */

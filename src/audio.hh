#pragma once

#include "adt/Opt.hh"
#include "adt/String.hh"
#include "adt/utils.hh"
#include "defaults.hh"
#include "mpris.hh"

#include <atomic>

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
    void (*seekMS)(IMixer* s, u64 ms);
    void (*seekLeftMS)(IMixer* s, u64 ms);
    void (*seekRightMS)(IMixer* s, u64 ms);
    Opt<String> (*getMetadata)(IMixer* s, const String sKey);
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
};

ADT_NO_UB constexpr void MixerInit(IMixer* s) { s->pVTable->init(s); }
ADT_NO_UB constexpr void MixerDestroy(IMixer* s) { s->pVTable->destroy(s); }
ADT_NO_UB constexpr void MixerPlay(IMixer* s, String sPath) { s->pVTable->play(s, sPath); }
ADT_NO_UB constexpr void MixerPause(IMixer* s, bool bPause) { s->pVTable->pause(s, bPause); }
ADT_NO_UB constexpr void MixerTogglePause(IMixer* s) { s->pVTable->togglePause(s); }
ADT_NO_UB constexpr void MixerChangeSampleRate(IMixer* s, u64 sampleRate, bool bSave) { s->pVTable->changeSampleRate(s, sampleRate, bSave); }
ADT_NO_UB constexpr void MixerSeekMS(IMixer* s, u64 ms) { s->pVTable->seekMS(s, ms); }
ADT_NO_UB constexpr void MixerSeekLeftMS(IMixer* s, u64 ms) { s->pVTable->seekLeftMS(s, ms); }
ADT_NO_UB constexpr void MixerSeekRightMS(IMixer* s, u64 ms) { s->pVTable->seekRightMS(s, ms); }
[[nodiscard]] ADT_NO_UB constexpr Opt<String> MixerGetMetadata(IMixer* s, const String sKey) { return s->pVTable->getMetadata(s, sKey); }

inline void
MixerSetVolume(IMixer* s, const f32 volume)
{
    s->volume = utils::clamp(volume, 0.0f, defaults::MAX_VOLUME);
    mpris::volumeChanged();
}

inline void MixerVolumeDown(IMixer* s, const f32 step) { MixerSetVolume(s, s->volume - step); }
inline void MixerVolumeUp(IMixer* s, const f32 step) { MixerSetVolume(s, s->volume + step); }
inline f64 MixerGetCurrentMS(IMixer* s) { return (f64(s->currentTimeStamp) / f64(s->sampleRate) / f64(s->nChannels)) * 1000.0; }
inline f64 MixerGetMaxMS(IMixer* s) { return (f64(s->totalSamplesCount) / f64(s->sampleRate) / f64(s->nChannels)) * 1000.0; }
inline void MixerChangeSampleRateDown(IMixer* s, u64 ms, bool bSave) { MixerChangeSampleRate(s, s->changedSampleRate - ms, bSave); }
inline void MixerChangeSampleRateUp(IMixer* s, u64 ms, bool bSave) { MixerChangeSampleRate(s, s->changedSampleRate + ms, bSave); }
inline void MixerRestoreSampleRate(IMixer* s) { MixerChangeSampleRate(s, s->sampleRate, false); }

struct DummyMixer
{
    IMixer base;

    constexpr DummyMixer();
};

constexpr void DummyMixerInit([[maybe_unused]] DummyMixer* s) {}
constexpr void DummyMixerDestroy([[maybe_unused]] DummyMixer* s) {}
constexpr void DummyMixerPlay([[maybe_unused]] DummyMixer* s, [[maybe_unused]] String sPath) {}
constexpr void DummyMixerPause([[maybe_unused]] DummyMixer* s, [[maybe_unused]] bool bPause) {}
constexpr void DummyMixerTogglePause([[maybe_unused]] DummyMixer* s) {}
constexpr void DummyMixerChangeSampleRate([[maybe_unused]] DummyMixer* s, [[maybe_unused]] u64 sampleRate, [[maybe_unused]] bool bSave) {}
constexpr long DummyMixerGetCurrentMS([[maybe_unused]] IMixer* s) { return {}; }
constexpr long DummyMixerGetMaxMS([[maybe_unused]] IMixer* s) { return {}; }
constexpr void DummyMixerSeekMS([[maybe_unused]] DummyMixer* s, [[maybe_unused]] u64 ms) {}
constexpr void DummyMixerSeekLeftMS([[maybe_unused]] DummyMixer* s, [[maybe_unused]] u64 ms) {}
constexpr void DummyMixerSeekRightMS([[maybe_unused]] DummyMixer* s, [[maybe_unused]] u64 ms) {}
constexpr Opt<String> DummyMixerGetMetadata([[maybe_unused]] IMixer* s, [[maybe_unused]] const String sKey) { return {}; }

inline const MixerVTable inl_DummyMixerVTable {
    .init = decltype(MixerVTable::init)(DummyMixerInit),
    .destroy = decltype(MixerVTable::destroy)(DummyMixerDestroy),
    .play = decltype(MixerVTable::play)(DummyMixerPlay),
    .pause = decltype(MixerVTable::pause)(DummyMixerPause),
    .togglePause = decltype(MixerVTable::togglePause)(DummyMixerTogglePause),
    .changeSampleRate = decltype(MixerVTable::changeSampleRate)(DummyMixerChangeSampleRate),
    .seekMS = decltype(MixerVTable::seekMS)(DummyMixerSeekMS),
    .seekLeftMS = decltype(MixerVTable::seekMS)(DummyMixerSeekLeftMS),
    .seekRightMS = decltype(MixerVTable::seekMS)(DummyMixerSeekRightMS),
    .getMetadata = decltype(MixerVTable::getMetadata)(DummyMixerGetMetadata),
};

constexpr
DummyMixer::DummyMixer()
    : base(&inl_DummyMixerVTable) {}

} /* namespace audio */

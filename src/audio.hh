#pragma once

#include "adt/Option.hh"
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
struct Mixer;
struct Track;

struct MixerInterface
{
    void (*init)(Mixer* s);
    void (*destroy)(Mixer*);
    void (*play)(Mixer*, String);
    void (*pause)(Mixer* s, bool bPause);
    void (*togglePause)(Mixer* s);
    void (*changeSampleRate)(Mixer* s, int sampleRate, bool bSave);
    void (*seekMS)(Mixer* s, u64 ms);
    void (*seekLeftMS)(Mixer* s, u64 ms);
    void (*seekRightMS)(Mixer* s, u64 ms);
    Option<String> (*getMetadata)(Mixer* s, const String sKey);
};

struct Mixer
{
    const MixerInterface* pVTable {};
    std::atomic<bool> bPaused = false;
#ifdef MPRIS_LIB
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

ADT_NO_UB constexpr void MixerInit(Mixer* s) { s->pVTable->init(s); }
ADT_NO_UB constexpr void MixerDestroy(Mixer* s) { s->pVTable->destroy(s); }
ADT_NO_UB constexpr void MixerPlay(Mixer* s, String sPath) { s->pVTable->play(s, sPath); }
ADT_NO_UB constexpr void MixerPause(Mixer* s, bool bPause) { s->pVTable->pause(s, bPause); }
ADT_NO_UB constexpr void MixerTogglePause(Mixer* s) { s->pVTable->togglePause(s); }
ADT_NO_UB constexpr void MixerChangeSampleRate(Mixer* s, int sampleRate, bool bSave) { s->pVTable->changeSampleRate(s, sampleRate, bSave); }
ADT_NO_UB constexpr void MixerSeekMS(Mixer* s, u64 ms) { s->pVTable->seekMS(s, ms); }
ADT_NO_UB constexpr void MixerSeekLeftMS(Mixer* s, u64 ms) { s->pVTable->seekLeftMS(s, ms); }
ADT_NO_UB constexpr void MixerSeekRightMS(Mixer* s, u64 ms) { s->pVTable->seekRightMS(s, ms); }
[[nodiscard]] ADT_NO_UB constexpr Option<String> MixerGetMetadata(Mixer* s, const String sKey) { return s->pVTable->getMetadata(s, sKey); }

inline void
MixerSetVolume(Mixer* s, const f32 volume)
{
    s->volume = utils::clamp(volume, 0.0f, defaults::MAX_VOLUME);
    mpris::volumeChanged();
}

inline void MixerVolumeDown(Mixer* s, const f32 step) { MixerSetVolume(s, s->volume - step); }
inline void MixerVolumeUp(Mixer* s, const f32 step) { MixerSetVolume(s, s->volume + step); }
inline f64 MixerGetCurrentMS(Mixer* s) { return (f64(s->currentTimeStamp) / f64(s->sampleRate) / f64(s->nChannels)) * 1000.0; }
inline f64 MixerGetMaxMS(Mixer* s) { return (f64(s->totalSamplesCount) / f64(s->sampleRate) / f64(s->nChannels)) * 1000.0; }

struct DummyMixer
{
    Mixer base;

    constexpr DummyMixer();
};

constexpr void DummyMixerInit([[maybe_unused]] DummyMixer* s) {}
constexpr void DummyMixerDestroy([[maybe_unused]] DummyMixer* s) {}
constexpr void DummyMixerPlay([[maybe_unused]] DummyMixer* s, [[maybe_unused]] String sPath) {}
constexpr void DummyMixerPause([[maybe_unused]] DummyMixer* s, [[maybe_unused]] bool bPause) {}
constexpr void DummyMixerTogglePause([[maybe_unused]] DummyMixer* s) {}
constexpr void DummyMixerChangeSampleRate([[maybe_unused]] DummyMixer* s, [[maybe_unused]] int sampleRate, [[maybe_unused]] bool bSave) {}
constexpr long DummyMixerGetCurrentMS([[maybe_unused]] Mixer* s) { return {}; }
constexpr long DummyMixerGetMaxMS([[maybe_unused]] Mixer* s) { return {}; }
constexpr void DummyMixerSeekMS([[maybe_unused]] DummyMixer* s, [[maybe_unused]] u64 ms) {}
constexpr void DummyMixerSeekLeftMS([[maybe_unused]] DummyMixer* s, [[maybe_unused]] u64 ms) {}
constexpr void DummyMixerSeekRightMS([[maybe_unused]] DummyMixer* s, [[maybe_unused]] u64 ms) {}
constexpr Option<String> DummyMixerGetMetadata([[maybe_unused]] Mixer* s, [[maybe_unused]] const String sKey) { return {}; }

inline const MixerInterface inl_DummyMixerVTable {
    .init = decltype(MixerInterface::init)(DummyMixerInit),
    .destroy = decltype(MixerInterface::destroy)(DummyMixerDestroy),
    .play = decltype(MixerInterface::play)(DummyMixerPlay),
    .pause = decltype(MixerInterface::pause)(DummyMixerPause),
    .togglePause = decltype(MixerInterface::togglePause)(DummyMixerTogglePause),
    .changeSampleRate = decltype(MixerInterface::changeSampleRate)(DummyMixerChangeSampleRate),
    .seekMS = decltype(MixerInterface::seekMS)(DummyMixerSeekMS),
    .seekLeftMS = decltype(MixerInterface::seekMS)(DummyMixerSeekLeftMS),
    .seekRightMS = decltype(MixerInterface::seekMS)(DummyMixerSeekRightMS),
    .getMetadata = decltype(MixerInterface::getMetadata)(DummyMixerGetMetadata),
};

constexpr
DummyMixer::DummyMixer()
    : base(&inl_DummyMixerVTable) {}

} /* namespace audio */

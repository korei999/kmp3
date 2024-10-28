#pragma once

#include "adt/String.hh"

#include <atomic>

using namespace adt;

namespace audio
{

constexpr u64 CHUNK_SIZE = (1 << 18); /* big enough */
constexpr u32 MAX_TRACK_COUNT = 8;

extern f32 g_globalVolume;

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
};

struct Mixer
{
    const MixerInterface* pVTable {};
    std::atomic<bool> bPaused = false;
    bool bMuted = false;
    bool bRunning = true;
    u32 sampleRate = 48000;
    u32 changedSampleRate = 48000;
    f32 volume = 0.5f;
};

ADT_NO_UB constexpr void MixerInit(Mixer* s) { s->pVTable->init(s); }
ADT_NO_UB constexpr void MixerDestroy(Mixer* s) { s->pVTable->destroy(s); }
ADT_NO_UB constexpr void MixerPlay(Mixer* s, String sPath) { s->pVTable->play(s, sPath); }
ADT_NO_UB constexpr void MixerPause(Mixer* s, bool bPause) { s->pVTable->pause(s, bPause); }
ADT_NO_UB constexpr void MixerTogglePause(Mixer* s) { s->pVTable->togglePause(s); }
ADT_NO_UB constexpr void MixerChangeSampleRate(Mixer* s, int sampleRate, bool bSave) { s->pVTable->changeSampleRate(s, sampleRate, bSave); }

struct DummyMixer
{
    Mixer base;

    constexpr DummyMixer();
};

constexpr void
DummyMixerInit([[maybe_unused]] DummyMixer* s, [[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    //
}

constexpr void
DummyMixerDestroy([[maybe_unused]] DummyMixer* s)
{
    //
}

constexpr void
DummyMixerPlay([[maybe_unused]] DummyMixer* s, [[maybe_unused]] String sPath)
{
    //
}

constexpr void
DummyMixerPause([[maybe_unused]] DummyMixer* s, [[maybe_unused]] bool bPause)
{
    //
}

constexpr void
DummyMixerTogglePause([[maybe_unused]] DummyMixer* s, [[maybe_unused]] bool bPause)
{
    //
}

constexpr void
DummyMixerChangeSampleRate([[maybe_unused]] DummyMixer* s, [[maybe_unused]] int sampleRate, [[maybe_unused]] bool bSave)
{
    //
}

inline const MixerInterface inl_DummyMixerVTable {
    .init = decltype(MixerInterface::init)(DummyMixerInit),
    .destroy = decltype(MixerInterface::destroy)(DummyMixerDestroy),
    .play = decltype(MixerInterface::play)(DummyMixerPlay),
    .pause = decltype(MixerInterface::pause)(DummyMixerPause),
    .togglePause = decltype(MixerInterface::togglePause)(DummyMixerTogglePause),
    .changeSampleRate = decltype(MixerInterface::changeSampleRate)(DummyMixerChangeSampleRate),
};

constexpr
DummyMixer::DummyMixer()
    : base(&inl_DummyMixerVTable) {}

} /* namespace audio */

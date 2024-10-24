#pragma once

#include "adt/String.hh"
#include "ffmpeg.hh"

using namespace adt;

namespace audio
{

constexpr u64 CHUNK_SIZE = 0x2FFFF; /* big enough */
constexpr u32 MAX_TRACK_COUNT = 8;

extern f32 g_globalVolume;

/* Platrform abstracted audio interface */
struct Mixer;
struct Track;

struct MixerInterface
{
    void (*init)(Mixer* s);
    void (*destroy)(Mixer*);
    void (*add)(Mixer*, Track);
    void (*addBackground)(Mixer*, Track);
    void (*play)(Mixer*, String);
};

struct Track
{
    ffmpeg::Decoder* pDecoder {};
    f32* pData = nullptr;
    u32 pcmPos = 0;
    u32 pcmSize = 0;
    u8 nChannels = 0;
    bool bRepeat = false;
    f32 volume = 0.0f;
};

struct Mixer
{
    const MixerInterface* pVTable {};
    bool bPaused = false;
    bool bMuted = false;
    bool bRunning = true;
    f32 volume = 0.5f;
};

ADT_NO_UB constexpr void MixerInit(Mixer* s) { s->pVTable->init(s); }
ADT_NO_UB constexpr void MixerDestroy(Mixer* s) { s->pVTable->destroy(s); }
ADT_NO_UB constexpr void MixerAdd(Mixer* s, Track t) { s->pVTable->add(s, t); }
ADT_NO_UB constexpr void MixerAddBackground(Mixer* s, Track t) { s->pVTable->addBackground(s, t); }
ADT_NO_UB constexpr void MixerPlay(Mixer* s, String sPath) { s->pVTable->play(s, sPath); }

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
DummyMixerAdd([[maybe_unused]] DummyMixer* s, [[maybe_unused]] Track t)
{
    //
}

constexpr void
DummyMixerAddBackground([[maybe_unused]] DummyMixer* s, [[maybe_unused]] Track t)
{
    //
}

constexpr void
DummyMixerPlay([[maybe_unused]] DummyMixer* s, [[maybe_unused]] String sPath)
{
    //
}

inline const MixerInterface inl_DummyMixerVTable {
    .init = decltype(MixerInterface::init)(DummyMixerInit),
    .destroy = decltype(MixerInterface::destroy)(DummyMixerDestroy),
    .add = decltype(MixerInterface::add)(DummyMixerAdd),
    .addBackground = decltype(MixerInterface::addBackground)(DummyMixerAddBackground),
    .play = decltype(MixerInterface::play)(DummyMixerPlay),
};

constexpr
DummyMixer::DummyMixer()
    : base {&inl_DummyMixerVTable} {}

} /* namespace audio */

#pragma once

#include "audio.hh"
#include "adt/Vec.hh"
#include "ffmpeg.hh"

#include <atomic>
#include <threads.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

namespace platform
{
namespace pipewire
{

struct Mixer;

void MixerInit(Mixer* s);
void MixerDestroy(Mixer* s);
void MixerAdd(Mixer* s, audio::Track t);
void MixerAddBackground(Mixer* s, audio::Track t);
void MixerPlay(Mixer* s, String sPath);

inline const audio::MixerInterface inl_mixerVTable {
    .init = decltype(audio::MixerInterface::init)(MixerInit),
    .destroy = decltype(audio::MixerInterface::destroy)(MixerDestroy),
    .add = decltype(audio::MixerInterface::add)(MixerAdd),
    .addBackground = decltype(audio::MixerInterface::addBackground)(MixerAddBackground),
    .play = decltype(audio::MixerInterface::play)(MixerPlay),
};

struct Mixer
{
    audio::Mixer base;
    u32 sampleRate = 48000;
    u8 channels = 2;
    enum spa_audio_format eformat {};
    std::atomic<bool> bPlaying = false;
    ffmpeg::Decoder* pDecoder {};

    static const pw_stream_events s_streamEvents;
    pw_core* pCore = nullptr;
    pw_context* pCtx = nullptr;
    pw_main_loop* pLoop = nullptr;
    pw_stream* pStream = nullptr;
    u32 lastNFrames = 0;

    mtx_t mtxAdd {};
    mtx_t mtxDecoder {};
    Vec<audio::Track> aTracks {};
    u32 currentBackgroundTrackIdx = 0;
    Vec<audio::Track> aBackgroundTracks {};

    thrd_t threadLoop {};

    Mixer() = default;
    Mixer(Allocator* pA) : base {&inl_mixerVTable}, aTracks(pA, audio::MAX_TRACK_COUNT), aBackgroundTracks(pA, audio::MAX_TRACK_COUNT) {}
};

} /* namespace pipewire */
} /* namespace platform */

namespace audio
{

inline void MixerInit(platform::pipewire::Mixer* s) { platform::pipewire::MixerInit(s); }
inline void MixerDestroy(platform::pipewire::Mixer* s) { platform::pipewire::MixerDestroy(s); }
inline void MixerAdd(platform::pipewire::Mixer* s, Track t) { platform::pipewire::MixerAdd(s, t); }
inline void MixerAddBackground(platform::pipewire::Mixer* s, Track t) { platform::pipewire::MixerAddBackground(s, t); }
inline void MixerPlay(platform::pipewire::Mixer* s, String sPath) { platform::pipewire::MixerPlay(s, sPath); }

} /* namespace audio */

#pragma once

#include "audio.hh"
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
void MixerPlay(Mixer* s, String sPath);
void MixerPause(Mixer* s, bool bPause);
void MixerTogglePause(Mixer* s);

inline const audio::MixerInterface inl_MixerVTable {
    .init = decltype(audio::MixerInterface::init)(MixerInit),
    .destroy = decltype(audio::MixerInterface::destroy)(MixerDestroy),
    .play = decltype(audio::MixerInterface::play)(MixerPlay),
    .pause = decltype(audio::MixerInterface::pause)(MixerPause),
    .togglePause = decltype(audio::MixerInterface::togglePause)(MixerTogglePause),
};

struct Mixer
{
    audio::Mixer base;
    u32 sampleRate = 48000;
    u8 channels = 2;
    enum spa_audio_format eformat {};
    std::atomic<bool> bDecodes = false;
    ffmpeg::Decoder* pDecoder {};
    String sPath {};

    pw_core* pCore = nullptr;
    pw_context* pCtx = nullptr;
    pw_thread_loop* pThrdLoop {};
    pw_stream* pStream = nullptr;
    u32 nLastFrames = 0;

    mtx_t mtxAdd {};
    mtx_t mtxDecoder {};
    mtx_t mtxThrdLoop {};
    cnd_t cndThrdLoop {};

    thrd_t threadLoop {};

    Mixer() = default;
    Mixer(Allocator* pA) : base(&inl_MixerVTable), pDecoder(ffmpeg::DecoderAlloc(pA)) {}
};

} /* namespace pipewire */
} /* namespace platform */

namespace audio
{

inline void MixerInit(platform::pipewire::Mixer* s) { platform::pipewire::MixerInit(s); }
inline void MixerDestroy(platform::pipewire::Mixer* s) { platform::pipewire::MixerDestroy(s); }
inline void MixerPlay(platform::pipewire::Mixer* s, String sPath) { platform::pipewire::MixerPlay(s, sPath); }
inline void MixerPause(platform::pipewire::Mixer* s, bool bPause) { platform::pipewire::MixerPause(s, bPause); }
inline void MixerTogglePause(platform::pipewire::Mixer* s) { platform::pipewire::MixerTogglePause(s); }

} /* namespace audio */

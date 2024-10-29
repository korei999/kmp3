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
void MixerChangeSampleRate(Mixer* s, int sampleRate, bool bSave);
void MixerSeekMS(Mixer* s, long ms);
void MixerSeekLeftMS(Mixer* s, long ms);
void MixerSeekRightMS(Mixer* s, long ms);
Option<String> MixerGetMetadata(Mixer* s, const String sKey);

inline const audio::MixerInterface inl_MixerVTable {
    .init = decltype(audio::MixerInterface::init)(MixerInit),
    .destroy = decltype(audio::MixerInterface::destroy)(MixerDestroy),
    .play = decltype(audio::MixerInterface::play)(MixerPlay),
    .pause = decltype(audio::MixerInterface::pause)(MixerPause),
    .togglePause = decltype(audio::MixerInterface::togglePause)(MixerTogglePause),
    .changeSampleRate = decltype(audio::MixerInterface::changeSampleRate)(MixerChangeSampleRate),
    .seekMS = decltype(audio::MixerInterface::seekMS)(MixerSeekMS),
    .seekLeftMS = decltype(audio::MixerInterface::seekLeftMS)(MixerSeekLeftMS),
    .seekRightMS = decltype(audio::MixerInterface::seekRightMS)(MixerSeekRightMS),
    .getMetadata = decltype(audio::MixerInterface::getMetadata)(MixerGetMetadata),
};

struct Mixer
{
    audio::Mixer base {};
    u8 nChannels = 2;
    enum spa_audio_format eformat {};
    std::atomic<bool> bDecodes = false;
    ffmpeg::Decoder* pDecoder {};
    String sPath {};

    pw_thread_loop* pThrdLoop {};
    pw_stream* pStream {};
    u32 nLastFrames {};

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
inline void MixerChangeSampleRate(platform::pipewire::Mixer* s, int sampleRate, bool bSave) { platform::pipewire::MixerChangeSampleRate(s, sampleRate, bSave); }
inline void MixerSeekMS(platform::pipewire::Mixer* s, long ms) { platform::pipewire::MixerSeekMS(s, ms); }
inline void MixerSeekMSLeft(platform::pipewire::Mixer* s, long ms) { platform::pipewire::MixerSeekLeftMS(s, ms); }
inline void MixerSeekMSRight(platform::pipewire::Mixer* s, long ms) { platform::pipewire::MixerSeekRightMS(s, ms); }
inline Option<String> MixerGetMetadata(platform::pipewire::Mixer* s, const String sKey) { return platform::pipewire::MixerGetMetadata(s, sKey); }

} /* namespace audio */

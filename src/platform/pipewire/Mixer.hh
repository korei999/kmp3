#pragma once

#include "audio.hh"
#include "ffmpeg.hh"

#include <atomic>
#include <threads.h>

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#ifdef __clang__
    #pragma clang diagnostic pop
#elif defined __GNUC__
    #pragma GCC diagnostic pop
#endif

namespace platform
{
namespace pipewire
{

struct Mixer
{
    audio::IMixer base {};
    u8 nChannels = 2;
    enum spa_audio_format eformat {};
    std::atomic<bool> bDecodes = false;
    ffmpeg::Decoder* pDecoder {};
    String sPath {};

    pw_thread_loop* pThrdLoop {};
    pw_stream* pStream {};
    u32 nLastFrames {};

    mtx_t mtxDecoder {};

    thrd_t threadLoop {};

    Mixer() = default;
    Mixer(IAllocator* pA);
};

void MixerInit(Mixer* s);
void MixerDestroy(Mixer* s);
void MixerPlay(Mixer* s, String sPath);
void MixerPause(Mixer* s, bool bPause);
void MixerTogglePause(Mixer* s);
void MixerChangeSampleRate(Mixer* s, u64 sampleRate, bool bSave);
void MixerSeekMS(Mixer* s, long ms);
void MixerSeekLeftMS(Mixer* s, long ms);
void MixerSeekRightMS(Mixer* s, long ms);
Opt<String> MixerGetMetadata(Mixer* s, const String sKey);
Opt<ffmpeg::Image> MixerGetCover(Mixer* s);
void MixerSetVolume(Mixer* s, const f32 volume);

inline const audio::MixerVTable inl_MixerVTable {
    .init = decltype(audio::MixerVTable::init)(MixerInit),
    .destroy = decltype(audio::MixerVTable::destroy)(MixerDestroy),
    .play = decltype(audio::MixerVTable::play)(MixerPlay),
    .pause = decltype(audio::MixerVTable::pause)(MixerPause),
    .togglePause = decltype(audio::MixerVTable::togglePause)(MixerTogglePause),
    .changeSampleRate = decltype(audio::MixerVTable::changeSampleRate)(MixerChangeSampleRate),
    .seekMS = decltype(audio::MixerVTable::seekMS)(MixerSeekMS),
    .seekLeftMS = decltype(audio::MixerVTable::seekLeftMS)(MixerSeekLeftMS),
    .seekRightMS = decltype(audio::MixerVTable::seekRightMS)(MixerSeekRightMS),
    .getMetadata = decltype(audio::MixerVTable::getMetadata)(MixerGetMetadata),
    .getCover = decltype(audio::MixerVTable::getCover)(MixerGetCover),
    .setVolume = decltype(audio::MixerVTable::setVolume)(MixerSetVolume),
};

inline Mixer::Mixer(IAllocator* pA) : base(&inl_MixerVTable), pDecoder(ffmpeg::DecoderAlloc(pA)) {}

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
inline Opt<String> MixerGetMetadata(platform::pipewire::Mixer* s, const String sKey) { return platform::pipewire::MixerGetMetadata(s, sKey); }
inline f64 MixerGetCurrentMS(platform::pipewire::Mixer* s) { return audio::MixerGetCurrentMS(&s->base); }
inline f64 MixerGetMaxMS(platform::pipewire::Mixer* s) { return audio::MixerGetMaxMS(&s->base); }

} /* namespace audio */

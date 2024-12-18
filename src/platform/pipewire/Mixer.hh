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
    audio::IMixer super {};
    u8 nChannels = 2;
    enum spa_audio_format eformat {};
    std::atomic<bool> bDecodes = false;
    ffmpeg::Decoder* pDecoder {};
    String sPath {};

    pw_thread_loop* pThrdLoop {};
    pw_stream* pStream {};
    u32 nLastFrames {};
    mtx_t mtxDecoder {};
    
    /* */

    Mixer() = default;
    Mixer(IAllocator* pA);

    void init();
    void destroy();
    void play(String sPath);
    void pause(bool bPause);
    void togglePause();
    void changeSampleRate(u64 sampleRate, bool bSave);
    void seekMS(u64 ms);
    void seekLeftMS(u64 ms);
    void seekRightMS(u64 ms);
    [[nodiscard]] Opt<String> getMetadata(const String sKey);
    [[nodiscard]] Opt<ffmpeg::Image> getCoverImage();
    void setVolume(const f32 volume);
};

inline const audio::MixerVTable inl_MixerVTable {
    .init = decltype(audio::MixerVTable::init)(+[](Mixer* s) { s->init(); }),
    .destroy = decltype(audio::MixerVTable::destroy)(+[](Mixer* s) { s->destroy(); }),
    .play = decltype(audio::MixerVTable::play)(+[](Mixer* s, String sPath) { s->play(sPath); }),
    .pause = decltype(audio::MixerVTable::pause)(+[](Mixer* s, bool bPause) { s->pause(bPause); }),
    .togglePause = decltype(audio::MixerVTable::togglePause)(+[](Mixer* s) { s->togglePause(); }),
    .changeSampleRate = decltype(audio::MixerVTable::changeSampleRate)(+[](Mixer* s, u64 sampleRate, bool bSave) { s->changeSampleRate(sampleRate, bSave); }),
    .seekMS = decltype(audio::MixerVTable::seekMS)(+[](Mixer* s, u64 ms) { s->seekMS(ms); }),
    .seekLeftMS = decltype(audio::MixerVTable::seekLeftMS)(+[](Mixer* s, u64 ms) { s->seekLeftMS(ms); }),
    .seekRightMS = decltype(audio::MixerVTable::seekRightMS)(+[](Mixer* s, u64 ms) { s->seekRightMS(ms); }),
    .getMetadata = decltype(audio::MixerVTable::getMetadata)(+[](Mixer* s, const String sKey) { return s->getMetadata(sKey); }),
    .getCoverImage = decltype(audio::MixerVTable::getCoverImage)(+[](Mixer* s) { return s->getCoverImage(); }),
    .setVolume = decltype(audio::MixerVTable::setVolume)(+[](Mixer* s, const f32 volume) { s->setVolume(volume); }),
};

inline
Mixer::Mixer(IAllocator* pA) : super(&inl_MixerVTable), pDecoder(ffmpeg::DecoderAlloc(pA)) {}

} /* namespace pipewire */
} /* namespace platform */

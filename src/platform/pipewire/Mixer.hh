#pragma once

#include "adt/logs.hh"
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

    /* */

    u8 m_nChannels = 2;
    enum spa_audio_format m_eformat {};
    std::atomic<bool> m_bDecodes = false;
    audio::IReader* m_pIReader {};
    String m_sPath {};

    pw_thread_loop* m_pThrdLoop {};
    pw_stream* m_pStream {};
    u32 m_nLastFrames {};
    mtx_t m_mtxDecoder {};
    
    /* */

    Mixer() = default;
    Mixer(IAllocator* pA);

    /* */

    void init();
    void destroy();
    void play(String sPath);
    void pause(bool bPause);
    void togglePause();
    void changeSampleRate(u64 sampleRate, bool bSave);
    void seekMS(s64 ms);
    void seekLeftMS(s64 ms);
    void seekRightMS(s64 ms);
    [[nodiscard]] Opt<String> getMetadata(const String sKey);
    [[nodiscard]] Opt<Image> getCoverImage();
    void setVolume(const f32 volume);
};

inline const audio::MixerVTable inl_MixerVTable = audio::MixerVTableGenerate<Mixer>();

inline
Mixer::Mixer(IAllocator* pA)
    : super(&inl_MixerVTable)
{
    auto* p = (ffmpeg::Reader*)pA->zalloc(1, sizeof(ffmpeg::Reader));
    *p = ffmpeg::Reader(INIT_FLAG::INIT);
    m_pIReader = (audio::IReader*)p;
    LOG_BAD("allocated\n");
}

} /* namespace pipewire */
} /* namespace platform */

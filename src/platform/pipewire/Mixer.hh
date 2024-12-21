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

struct Mixer : public audio::IMixer
{
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

    virtual void init() override final;
    virtual void destroy() override final;
    virtual void play(String sPath) override final;
    virtual void pause(bool bPause) override final;
    virtual void togglePause() override final;
    virtual void changeSampleRate(u64 sampleRate, bool bSave) override final;
    virtual void seekMS(s64 ms) override final;
    virtual void seekLeftMS(s64 ms) override final;
    virtual void seekRightMS(s64 ms) override final;
    [[nodiscard]] virtual Opt<String> getMetadata(const String sKey) override final;
    [[nodiscard]] virtual Opt<Image> getCoverImage() override final;
    virtual void setVolume(const f32 volume) override final;
};

inline
Mixer::Mixer(IAllocator* pA)
{
    auto* p = (ffmpeg::Reader*)pA->zalloc(1, sizeof(ffmpeg::Reader));
    new(p) ffmpeg::Reader();

    m_pIReader = p;
}

} /* namespace pipewire */
} /* namespace platform */

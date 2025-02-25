#pragma once

#include "adt/Thread.hh"

#include "audio.hh"

#include <atomic>

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

namespace platform::pipewire
{

struct Device;

class Mixer : public audio::IMixer
{
protected:
    u8 m_nChannels = 2;
    enum spa_audio_format m_eformat {};
    std::atomic<bool> m_bDecodes = false;
    audio::IDecoder* m_pIDecoder {};
    StringView m_sPath {};

    pw_thread_loop* m_pThrdLoop {};
    pw_stream* m_pStream {};
    u32 m_nLastFrames {};
    f64 m_currMs {};
    Mutex m_mtxDecoder {};

    /* */

public:
    virtual void init() override final;
    virtual void destroy() override final;
    virtual void play(StringView sPath) override final;
    virtual void pause(bool bPause) override final;
    virtual void togglePause() override final;
    virtual void changeSampleRate(u64 sampleRate, bool bSave) override final;
    virtual void seekMS(f64 ms) override final;
    virtual void seekOff(f64 offset) override final;
    [[nodiscard]] virtual Opt<StringView> getMetadata(const StringView sKey) override final;
    [[nodiscard]] virtual Opt<Image> getCoverImage() override final;
    virtual void setVolume(const f32 volume) override final;
    [[nodiscard]] virtual i64 getCurrentMS() override final;
    [[nodiscard]] virtual i64 getTotalMS() override final;

    /* */

    static void* getOnProcessPFN() { return methodPointer(&Mixer::onProcess); }

    /* */

private:
    void writeFramesLocked(Span<f32> spBuff, u32 nFrames, long* pSamplesWritten, i64* pPcmPos);
    void setNChannles(u32 nChannles);
    void onProcess();
};

} /* namespace platform::pipewire */

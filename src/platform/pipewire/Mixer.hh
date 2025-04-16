#pragma once

#include "adt/Thread.hh"

#include "audio.hh"

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

namespace platform::pipewire
{

struct Device;

class Mixer : public audio::IMixer
{
protected:
    adt::u8 m_nChannels = 2;
    enum spa_audio_format m_eformat {};
    adt::atomic::Int m_atom_bDecodes {false};
    adt::StringView m_svPath {};

    pw_thread_loop* m_pThrdLoop {};
    pw_stream* m_pStream {};
    adt::u32 m_nLastFrames {};
    adt::f64 m_currMs {};
    adt::Mutex m_mtxDecoder {};

    /* */

public:
    virtual void init() override final;
    virtual void destroy() override final;
    virtual void play(adt::StringView sPath) override final;
    virtual void pause(bool bPause) override final;
    virtual void togglePause() override final;
    virtual void changeSampleRate(adt::u64 sampleRate, bool bSave) override final;
    virtual void seekMS(adt::f64 ms) override final;
    virtual void seekOff(adt::f64 offset) override final;
    virtual void setVolume(const adt::f32 volume) override final;
    [[nodiscard]] virtual adt::i64 getCurrentMS() override final;
    [[nodiscard]] virtual adt::i64 getTotalMS() override final;

    /* */

    static void* getOnProcessPFN() { return adt::methodPointer(&Mixer::onProcess); }

    /* */

private:
    void writeFramesLocked(adt::Span<adt::f32> spBuff, adt::u32 nFrames, long* pSamplesWritten, adt::i64* pPcmPos);
    void setNChannles(adt::u32 nChannles);
    void onProcess();
};

} /* namespace platform::pipewire */

#pragma once

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

struct Mixer : public audio::IMixer
{
    enum spa_audio_format m_eformat {};

    pw_thread_loop* m_pThrdLoop {};
    pw_stream* m_pStream {};

    /* */

    virtual Mixer& init() override final;
    virtual void deinit() override final;
    virtual bool play(StringView sPath) override final;
    virtual void pause(bool bPause) override final;
    virtual void changeSampleRate(u64 sampleRate, bool bSave) override final;

    /* */

    void setNChannels(u32 nChannels);
    void onProcess();
};

} /* namespace platform::pipewire */

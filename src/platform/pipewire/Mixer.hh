#pragma once

#include "audio.hh"

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

struct Device;

class Mixer : public audio::IMixer
{
protected:
    u8 m_nChannels = 2;
    enum spa_audio_format m_eformat {};
    std::atomic<bool> m_bDecodes = false;
    audio::IDecoder* m_pIDecoder {};
    String m_sPath {};

    pw_thread_loop* m_pThrdLoop {};
    pw_stream* m_pStream {};
    u32 m_nLastFrames {};
    mtx_t m_mtxDecoder {};
    spa_list m_lDevices {};
    spa_list m_lNodes {};
    pw_context* m_pCtx {};
    pw_core* m_pCore {};
    pw_registry* m_pRegistry {};
    spa_hook m_coreListener {};
    spa_hook m_registryListener {};
    spa_hook m_metadataListener {};

    void* m_pProxyMetadata {};
    void* m_pProxyNodeSink {};
    void* m_pProxyNodeSource {};

    f64 m_currMs {};

    /* */

public:

    /* */

    virtual void init() override final;
    virtual void destroy() override final;
    virtual void play(String sPath) override final;
    virtual void pause(bool bPause) override final;
    virtual void togglePause() override final;
    virtual void changeSampleRate(u64 sampleRate, bool bSave) override final;
    virtual void seekMS(f64 ms) override final;
    virtual void seekOff(f64 offset) override final;
    [[nodiscard]] virtual Opt<String> getMetadata(const String sKey) override final;
    [[nodiscard]] virtual Opt<Image> getCoverImage() override final;
    virtual void setVolume(const f32 volume) override final;
    [[nodiscard]] virtual s64 getCurrentMS() override final;
    [[nodiscard]] virtual s64 getTotalMS() override final;

    /* */

private:
    void writeFramesLocked(f32* pBuff, u32 nFrames, long* pSamplesWritten, s64* pPcmPos);
    void configureChannles(u32 nChannles);

    friend Device;
    friend void onProcess(void* pData);
    friend void registryGlobalRemove(void* pData, uint32_t id);
    friend void registryGlobal(void* pData, uint32_t id, uint32_t permissions, const char* pType, uint32_t version, const spa_dict* pProps);
    friend int metadataProperty(void* pData, uint32_t subject, const char* pKey, const char* pType, const char* pValue);
};

} /* namespace pipewire */
} /* namespace platform */

#include "Mixer.hh"

#include "adt/guard.hh"
#include "adt/logs.hh"
#include "adt/math.hh"
#include "adt/utils.hh"
#include "app.hh"
#include "defaults.hh"
#include "ffmpeg.hh"
#include "mpris.hh"

#include <cmath>

#include <emmintrin.h>
#include <immintrin.h>

#include <pipewire/impl-metadata.h>

namespace platform
{
namespace pipewire
{

struct PWLockGuard
{
    pw_thread_loop* p;

    PWLockGuard() = delete;
    PWLockGuard(pw_thread_loop* _p) : p(_p) { pw_thread_loop_lock(_p); }
    ~PWLockGuard() { pw_thread_loop_unlock(p); }
};

struct Device
{
    Mixer* pMix {};

    struct spa_list link;
    int id;
    struct spa_list routes;

    void* pProxy;
    struct spa_hook listener;
};

void onProcess(void* data);

void registryGlobalRemove(void* pData, uint32_t id);
void registryGlobal(void* pData, uint32_t id, uint32_t permissions, const char* pType, uint32_t version, const spa_dict* pProps);
int metadataProperty(void* pData, uint32_t subject, const char* pKey, const char* pType, const char* pValue);

static void coreDone(void* pData, uint32_t id, int seq);
static void deviceInfo(void* pData, const struct pw_device_info* pInfo);
static void deviceParam(void* pData, int seq, uint32_t id, uint32_t index, uint32_t next, const struct spa_pod* pParam);

static const pw_metadata_events s_metadataEvents {
    .version = PW_VERSION_METADATA_EVENTS,
    .property = metadataProperty,
};

static const pw_stream_events s_streamEvents {
    .version = PW_VERSION_STREAM_EVENTS,
    .destroy {},
    .state_changed {},
    .control_info {},
    .io_changed {},
    .param_changed {},
    .add_buffer {},
    .remove_buffer {},
    .process = onProcess,
    .drained {},
    .command {},
    .trigger_done {},
};

static const pw_registry_events s_registryEvents {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = registryGlobal,
    .global_remove = registryGlobalRemove,
};

static const pw_core_events s_coreEvents {
    .version = PW_VERSION_CORE_EVENTS,
    .info {},
    .done = coreDone,
    .ping {},
    .error {},
    .remove_id {},
    .bound_id {},
    .add_mem {},
    .remove_mem {},
    .bound_props {}
};

static const pw_device_events s_deviceEvents {
    .version = PW_VERSION_DEVICE_EVENTS,
    .info = deviceInfo,
    .param = deviceParam,
};

static f32 s_aPwBuff[audio::CHUNK_SIZE] {};

int
metadataProperty(
    [[maybe_unused]] void* pData,
    [[maybe_unused]] uint32_t subject,
    [[maybe_unused]] const char* pKey,
    [[maybe_unused]] const char* pType,
    [[maybe_unused]] const char* pValue
)
{
    [[maybe_unused]] Mixer* pMix = (Mixer*)pData;
    [[maybe_unused]] bool bSink = false;
    [[maybe_unused]] char** ppTargetName {};

    if (spa_streq(pKey, "default.audio.sink"))
    {
        bSink = true;
        LOG_BAD("\nkey: '{}'\ntype: '{}'\nvalue: '{}'\n", pKey, pType, pValue);
    }
    else if (spa_streq(pKey, "default.audio.source"))
    {
        bSink = false;
    }
    PW_KEY_CORE_ID;

    return 0;
}

static void
deviceInfo(void* pData, const struct pw_device_info* pInfo)
{
    Device* pDev = (Device*)pData;

    if (!(pInfo->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS))
        return;

    for (u32 i = 0; i < pInfo->n_params; ++i)
    {
        if (pInfo->params[i].id == SPA_PARAM_Route)
        {
            pw_device_enum_params(pDev->pProxy, 0, pInfo->params[i].id, 0, -1, {});
            return;
        }
    }
}

static void
deviceParam(
    [[maybe_unused]] void* pData,
    [[maybe_unused]] int seq,
    [[maybe_unused]] uint32_t id,
    [[maybe_unused]] uint32_t index,
    [[maybe_unused]] uint32_t next,
    [[maybe_unused]] const struct spa_pod* pParam)
{
}

static void
coreDone([[maybe_unused]] void* pData, [[maybe_unused]] uint32_t id, [[maybe_unused]] int seq)
{
}

[[maybe_unused]] static void
coreError(
    [[maybe_unused]] void* pData,
    [[maybe_unused]] uint32_t id,
    [[maybe_unused]] int seq,
    [[maybe_unused]] int res,
    [[maybe_unused]] const char* pMessage)
{
}

void
registryGlobal(
    [[maybe_unused]] void* pData,
    [[maybe_unused]] uint32_t id,
    [[maybe_unused]] uint32_t permissions,
    [[maybe_unused]] const char* pType,
    [[maybe_unused]] uint32_t version,
    [[maybe_unused]] const spa_dict* pProps
)
{
    Mixer* pMix = (Mixer*)(pData);

    if (spa_streq(pType, PW_TYPE_INTERFACE_Device))
    {
        Device* pDev = (Device*)::calloc(1, sizeof(*pDev));
        pDev->pMix = pMix;
        pDev->id = id;

        spa_list_init(&pDev->routes);

        pDev->pProxy = pw_registry_bind(pMix->m_pRegistry, id, pType, PW_VERSION_DEVICE, 0);
        assert(pDev->pProxy);
        pw_device_add_listener(pDev->pProxy, &pDev->listener, &s_deviceEvents, pDev);

        spa_list_append(&pMix->m_lDevices, &pDev->link);
    }
    else if (spa_streq(pType, PW_TYPE_INTERFACE_Metadata))
    {
        if (pMix->m_pProxyMetadata != nullptr) return;

        char const* pName = spa_dict_lookup(pProps, PW_KEY_METADATA_NAME);

        if (!spa_streq(pName, "default")) return;

        LOG_BAD("pName: '{}'\n", pName);
        pMix->m_pProxyMetadata = pw_registry_bind(pMix->m_pRegistry, id, pType, PW_VERSION_METADATA, 0);

        pw_metadata_add_listener(pMix->m_pProxyMetadata, &pMix->m_metadataListener, &s_metadataEvents, pMix);
    }
}

void
registryGlobalRemove([[maybe_unused]] void* pData, [[maybe_unused]] uint32_t id)
{
    [[maybe_unused]] auto* pMix = (Mixer*)pData;
}

static u32
formatByteSize(spa_audio_format eFormat)
{
    switch (eFormat)
    {
        default: return 1;

        case SPA_AUDIO_FORMAT_S16_LE:
        case SPA_AUDIO_FORMAT_S16_BE:
        case SPA_AUDIO_FORMAT_U16_LE:
        case SPA_AUDIO_FORMAT_U16_BE:
                 return 2;

        case SPA_AUDIO_FORMAT_S24_LE:
        case SPA_AUDIO_FORMAT_S24_BE:
        case SPA_AUDIO_FORMAT_U24_LE:
        case SPA_AUDIO_FORMAT_U24_BE:
                 return 3;

        case SPA_AUDIO_FORMAT_S24_32_LE:
        case SPA_AUDIO_FORMAT_S24_32_BE:
        case SPA_AUDIO_FORMAT_U24_32_LE:
        case SPA_AUDIO_FORMAT_U24_32_BE:
        case SPA_AUDIO_FORMAT_S32_LE:
        case SPA_AUDIO_FORMAT_S32_BE:
        case SPA_AUDIO_FORMAT_U32_LE:
        case SPA_AUDIO_FORMAT_U32_BE:
        case SPA_AUDIO_FORMAT_F32_LE:
        case SPA_AUDIO_FORMAT_F32_BE:
                 return 4;

        case SPA_AUDIO_FORMAT_F64_LE:
        case SPA_AUDIO_FORMAT_F64_BE:
                 return 8;
    };
}

void
Mixer::init()
{
    m_pIDecoder = (ffmpeg::Decoder*)calloc(1, sizeof(ffmpeg::Decoder));
    new(m_pIDecoder) ffmpeg::Decoder();

    m_bRunning = true;
    m_bMuted = false;
    m_volume = 0.1f;

    m_sampleRate = 48000;
    m_nChannels = 2;
    m_eformat = SPA_AUDIO_FORMAT_F32;

    mtx_init(&m_mtxDecoder, mtx_plain);

    pw_init(&app::g_argc, &app::g_argv);

    spa_list_init(&m_lDevices);

    u8 setupBuffer[1024] {};
    const spa_pod* aParams[1] {};
    spa_pod_builder b {};
    spa_pod_builder_init(&b, setupBuffer, sizeof(setupBuffer));

    m_pThrdLoop = pw_thread_loop_new("kmp3PwThreadLoop", {});

    m_pCtx = pw_context_new(pw_thread_loop_get_loop(m_pThrdLoop), {}, {});
    m_pCore = pw_context_connect(m_pCtx, {}, {});

    pw_core_add_listener(m_pCore, &m_coreListener, &s_coreEvents, this);

    m_pRegistry = pw_core_get_registry(m_pCore, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(m_pRegistry, &m_registryListener, &s_registryEvents, this);

    m_pStream = pw_stream_new_simple(
        pw_thread_loop_get_loop(m_pThrdLoop),
        "kmp3AudioSource",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "Music",
            nullptr
        ),
        &s_streamEvents,
        this
    );

    spa_audio_info_raw rawInfo {
        .format = m_eformat,
        .flags {},
        .rate = m_sampleRate,
        .channels = m_nChannels,
        .position {}
    };

    aParams[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    pw_stream_connect(
        m_pStream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT|PW_STREAM_FLAG_INACTIVE|PW_STREAM_FLAG_MAP_BUFFERS),
        aParams,
        utils::size(aParams)
    );

    pw_thread_loop_start(m_pThrdLoop);
}

void
Mixer::destroy()
{
    {
        PWLockGuard tLock(m_pThrdLoop);
        pw_stream_set_active(m_pStream, true);

        {
            Device* it {}, * tmp {};
            spa_list_for_each_safe(it, tmp, &m_lDevices, link)
            {
                pw_proxy_destroy((pw_proxy*)it->pProxy);
                spa_list_remove(&it->link);
                ::free(it);
            }
        }

        if (m_pProxyMetadata) pw_proxy_destroy((pw_proxy*)m_pProxyMetadata);
        if (m_pRegistry) pw_proxy_destroy((pw_proxy*)m_pRegistry);
    }

    pw_thread_loop_stop(m_pThrdLoop);
    LOG_NOTIFY("pw_thread_loop_stop()\n");

    if (m_pCtx) pw_context_destroy(m_pCtx);

    m_bRunning = false;

    if (m_bDecodes) m_pIDecoder->close();

    if (m_pStream) pw_stream_destroy(m_pStream);
    if (m_pThrdLoop) pw_thread_loop_destroy(m_pThrdLoop);
    pw_deinit();

    mtx_destroy(&m_mtxDecoder);
    LOG_NOTIFY("MixerDestroy()\n");

    ::free(m_pIDecoder);
}

void
Mixer::play(String sPath)
{
    const f64 prevSpeed = f64(m_changedSampleRate) / f64(m_sampleRate);

    pause(true);

    {
        guard::Mtx lockDec(&m_mtxDecoder);

        m_sPath = sPath;

        if (m_bDecodes)
        {
            m_pIDecoder->close();
        }

        auto err = m_pIDecoder->open(sPath);
        if (err != audio::ERROR::OK_)
        {
            LOG_WARN("DecoderOpen\n");
            return;
        }
        m_bDecodes = true;

    }

    m_nChannels = m_pIDecoder->getChannelsCount();
    changeSampleRate(m_pIDecoder->getSampleRate(), true);

    if (!math::eq(prevSpeed, 1.0))
        changeSampleRate(f64(m_sampleRate) * prevSpeed, false);

    pause(false);

    m_bUpdateMpris = true; /* mark to update in frame::run() */
}

void
Mixer::writeFramesLocked(f32* pBuff, u32 nFrames, long* pSamplesWritten, s64* pPcmPos)
{
    audio::ERROR err {};
    {
        guard::Mtx lock(&m_mtxDecoder);

        if (!m_bDecodes) return;

        err = m_pIDecoder->writeToBuffer(
            pBuff, utils::size(s_aPwBuff),
            nFrames, m_nChannels,
            pSamplesWritten, pPcmPos
        );
        if (err == audio::ERROR::END_OF_FILE)
        {
            pause(true);
            m_pIDecoder->close();
            m_bDecodes = false;
        }
    }

    if (err == audio::ERROR::END_OF_FILE) app::g_pPlayer->onSongEnd();
}

void
Mixer::configureChannles([[maybe_unused]] u32 nChannles)
{
}

void
onProcess(void* pData)
{
    auto* self = (Mixer*)pData;

    pw_buffer* pPwBuffer = pw_stream_dequeue_buffer(self->m_pStream);
    if (!pPwBuffer)
    {
        pw_log_warn("out of buffers: %m");
        return;
    }

    auto pBuffData = pPwBuffer->buffer->datas[0];
    auto* pDest = (f32*)pBuffData.data;

    if (!pDest)
    {
        LOG_WARN("pDest == nullptr\n");
        return;
    }

    u32 stride = formatByteSize(self->m_eformat) * self->m_nChannels;
    u32 nFrames = pBuffData.maxsize / stride;
    if (pPwBuffer->requested) nFrames = SPA_MIN(pPwBuffer->requested, (u64)nFrames);

    if (nFrames * self->m_nChannels > utils::size(s_aPwBuff)) nFrames = utils::size(s_aPwBuff);

    self->m_nLastFrames = nFrames;

    static long s_nDecodedSamples = 0;
    static long s_nWrites = 0;

    f32 vol = self->m_bMuted ? 0.0f : std::pow(self->m_volume, 3);

    for (u32 frameIdx = 0; frameIdx < nFrames; frameIdx++)
    {
        for (u32 chIdx = 0; chIdx < self->m_nChannels; chIdx++)
        {
            /* modify each sample here */
            *pDest++ = s_aPwBuff[s_nWrites++] * vol;
        }

        if (s_nWrites >= s_nDecodedSamples)
        {
            /* ask to fill the buffer when it's empty */
            self->writeFramesLocked(s_aPwBuff, nFrames, &s_nDecodedSamples, &self->m_currentTimeStamp);
            self->m_currMs = self->m_pIDecoder->getCurrentMS();
            s_nWrites = 0;
        }
    }

    if (s_nDecodedSamples == 0)
    {
        self->m_currentTimeStamp = 0;
        self->m_nTotalSamples = 0;
    }
    else
    {
        self->m_nTotalSamples = self->m_pIDecoder->getTotalSamplesCount();
    }

    pBuffData.chunk->offset = 0;
    pBuffData.chunk->stride = stride;
    pBuffData.chunk->size = nFrames * stride;

    pw_stream_queue_buffer(self->m_pStream, pPwBuffer);
}

void
Mixer::pause(bool bPause)
{
    PWLockGuard lock(m_pThrdLoop);
    pw_stream_set_active(m_pStream, !bPause);
    m_bPaused = bPause;

    LOG_NOTIFY("bPaused: {}\n", m_bPaused);
    mpris::playbackStatusChanged();
}

void
Mixer::togglePause()
{
    pause(!m_bPaused);
}

void
Mixer::changeSampleRate(u64 sampleRate, bool bSave)
{
    sampleRate = utils::clamp(sampleRate, defaults::MIN_SAMPLE_RATE, defaults::MAX_SAMPLE_RATE);

    u8 aSetupBuff[512] {};
    spa_audio_info_raw rawInfo {
        .format = m_eformat,
        .flags {},
        .rate = static_cast<u32>(sampleRate),
        .channels = m_nChannels,
        .position {}
    };

    spa_pod_builder b {};
    spa_pod_builder_init(&b, aSetupBuff, sizeof(aSetupBuff));

    const spa_pod* aParams[1] {};
    aParams[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    PWLockGuard lock(m_pThrdLoop);
    pw_stream_update_params(m_pStream, aParams, utils::size(aParams));
    /* update won't apply without this */
    pw_stream_set_active(m_pStream, m_bPaused);
    pw_stream_set_active(m_pStream, !m_bPaused);

    if (bSave) m_sampleRate = sampleRate;

    m_changedSampleRate = sampleRate;
}

void
Mixer::seekMS(f64 ms)
{
    guard::Mtx lock(&m_mtxDecoder);
    if (!m_bDecodes) return;

    ms = utils::clamp(ms, 0.0, (f64)getTotalMS());
    m_pIDecoder->seekMS(ms);

    m_currMs = ms;
    m_currentTimeStamp = (ms * m_sampleRate * m_nChannels) / 1000.0;
    m_nTotalSamples = m_pIDecoder->getTotalSamplesCount();

    mpris::seeked();
}

void
Mixer::seekOff(f64 offset)
{
    auto time = m_pIDecoder->getCurrentMS() + offset;
    seekMS(time);
}

Opt<String>
Mixer::getMetadata(const String sKey)
{
    return m_pIDecoder->getMetadataValue(sKey);
}

Opt<Image>
Mixer::getCoverImage()
{
    return m_pIDecoder->getCoverImage();
}

void
Mixer::setVolume(const f32 volume)
{
    m_volume = utils::clamp(volume, 0.0f, defaults::MAX_VOLUME);
    mpris::volumeChanged();
}

s64
Mixer::getCurrentMS()
{
    return m_currMs;
}

s64
Mixer::getTotalMS()
{
    return m_pIDecoder->getTotalMS();
}

} /* namespace pipewire */
} /* namespace platform */

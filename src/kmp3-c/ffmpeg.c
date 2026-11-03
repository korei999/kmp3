#include "ffmpeg.h"

#include "klib/PP.h"
#include "klib/Ctx.h"
#include "klib/Gpa.h"

#include <libavutil/imgutils.h>

#include <dlfcn.h>

static void* ffmpeg_s_pLibavformat;

static void* ffmpeg_tryLoad(const k_StringView svDLLName);
static bool ffmpeg_loadLibs(void);
static void ffmpeg_unloadLibs(void);

#ifdef OPT_CHAFA

static void ffmpeg_DecoderGetAttachedPicture(ffmpeg_Decoder* s);
static void* ffmpeg_s_pLiblibswscale;
static IMAGE_PIXEL_FORMAT ffmpeg_convertFormat(int format);

#else
    #define ffmpeg_DecoderGetAttachedPicture(...) (void)0
#endif

/* Load all needed ffmpeg functions by hand to save some startup time. */

#define FFMPEG_DLL_PFN(FUNC) K_TYPEOF(FUNC)* ffmpeg_##FUNC;

#define FFMPEG_DLL_CORE_PFN_LIST \
    avformat_close_input,\
    av_packet_free,\
    av_frame_free,\
\
    av_dict_get,\
\
    avcodec_free_context,\
    avcodec_find_decoder,\
    avcodec_alloc_context3,\
\
    avcodec_parameters_to_context,\
    avcodec_open2,\
\
    av_packet_alloc,\
    av_frame_alloc,\
    av_read_frame,\
    avcodec_send_packet,\
    avcodec_receive_frame,\
\
    swr_alloc_set_opts2,\
    swr_config_frame,\
    swr_free,\
    swr_convert_frame,\
\
    av_image_fill_linesizes,\
    av_frame_get_buffer,\
\
    av_log_set_level,\
    avformat_open_input,\
    avformat_find_stream_info,\
\
    av_find_best_stream,\
\
    avcodec_flush_buffers,\
    av_rescale_q,\
    av_seek_frame,\
    av_packet_unref,\
\
    av_frame_unref,\
    av_strerror

K_PP_FOR_EACH(FFMPEG_DLL_PFN, FFMPEG_DLL_CORE_PFN_LIST)

#ifdef OPT_CHAFA

#define FFMPEG_DLL_CHAFA_PFN_LIST \
    sws_getContext,\
    sws_scale_frame,\
    sws_freeContext

K_PP_FOR_EACH(FFMPEG_DLL_PFN, FFMPEG_DLL_CHAFA_PFN_LIST)

#endif

static void*
ffmpeg_tryLoad(const k_StringView svDLLName)
{
    const k_StringView aPaths[] = {
        K_SV(""),
        K_SV("/usr/local/lib/"),
        K_SV("/usr/lib/"),
        K_SV("/opt/local/lib/"),
        K_SV("/opt/homebrew/lib/"),
    };

    k_Arena* pArena = k_CtxArena();
    void* pRet = NULL;

    for (ssize_t i = 0; i < K_ASIZE(aPaths); ++i)
    {
        k_ArenaState arenaState = k_ArenaStatePush(pArena);

        k_String s = k_StringCreateSv(&pArena->base, aPaths[i]);
        k_StringPushSv(&s, &pArena->base, svDLLName);

        pRet = dlopen(k_StringData(&s), RTLD_NOW | RTLD_LOCAL);

        k_ArenaStateRestore(&arenaState);
        if (pRet) return pRet;
    }

    return NULL;
}

static bool
ffmpeg_loadLibs(void)
{
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wpedantic"
#else
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define SYM(lib, name)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        ffmpeg_##name = dlsym(lib, #name);                                                                             \
        if (!ffmpeg_##name)                                                                                            \
        {                                                                                                              \
            K_CTX_LOG_DEBUG("failed to load: '{}'\n", #name);                                                          \
            dlclose(lib);                                                                                              \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)

    {

#ifdef __APPLE__
        s_pLibavformat = ffmpeg_tryLoad(K_SV("libavformat.dylib"));
#else
        ffmpeg_s_pLibavformat = ffmpeg_tryLoad(K_SV("libavformat.so"));
#endif
        if (!ffmpeg_s_pLibavformat)
            return false;

#define _SYM(FUNC) SYM(ffmpeg_s_pLibavformat, FUNC);
        K_PP_FOR_EACH(_SYM, FFMPEG_DLL_CORE_PFN_LIST);
#undef _SYM
    }

    {
#ifdef OPT_CHAFA

    #ifdef __APPLE__
        ffmpeg_s_pLiblibswscale = ffmpeg_tryLoad(K_SV("libswscale.dylib"));
    #else
        ffmpeg_s_pLiblibswscale = ffmpeg_tryLoad(K_SV("libswscale.so"));
    #endif
        if (!ffmpeg_s_pLiblibswscale)
            return false;

    #define _SYM(FUNC) SYM(ffmpeg_s_pLiblibswscale, FUNC);
        K_PP_FOR_EACH(_SYM, FFMPEG_DLL_CHAFA_PFN_LIST);
    #undef _SYM

#endif
    }

    return true;

#ifdef __clang__
    #pragma clang diagnostic pop
#else
    #pragma GCC diagnostic pop
#endif
}

static void
ffmpeg_unloadLibs(void)
{
    if (ffmpeg_s_pLibavformat) dlclose(ffmpeg_s_pLibavformat), ffmpeg_s_pLibavformat = NULL;
#ifdef OPT_CHAFA
    if (ffmpeg_s_pLiblibswscale) dlclose(ffmpeg_s_pLiblibswscale), ffmpeg_s_pLiblibswscale = NULL;
#endif
}

#ifdef OPT_CHAFA

static void
ffmpeg_DecoderGetAttachedPicture(ffmpeg_Decoder* s)
{
    int err = 0;
    AVStream* pStream = NULL;

    for (int i = 0; i < (int)s->pFormatCtx->nb_streams; ++i)
    {
        AVStream* itStream = s->pFormatCtx->streams[i];
        if (itStream->disposition & AV_DISPOSITION_ATTACHED_PIC)
        {
            K_CTX_LOG_DEBUG("Found 'attached_pic'");
            pStream = itStream;
            break;
        }
    }

    if (!pStream) return;

    const AVCodec* pCodec = ffmpeg_avcodec_find_decoder(pStream->codecpar->codec_id);
    if (!pCodec) return;

    AVCodecContext* pCodecCtx = ffmpeg_avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) return;

    ffmpeg_avcodec_parameters_to_context(pCodecCtx, pStream->codecpar);
    err = ffmpeg_avcodec_open2(pCodecCtx, pCodec, NULL);
    if (err < 0)
    {
        K_CTX_LOG_DEBUG("avcodec_open2()");
        goto cleanup;
    }

    K_CTX_LOG_DEBUG("codec name: '{nts}'", pCodec->long_name);

    s->pImgPacket = ffmpeg_av_packet_alloc();
    err = ffmpeg_av_read_frame(s->pFormatCtx, s->pImgPacket);
    if (err != 0) goto cleanup;

    err = ffmpeg_avcodec_send_packet(pCodecCtx, s->pImgPacket);
    if (err != 0) goto cleanup;

    s->pImgFrame = ffmpeg_av_frame_alloc();
    err = ffmpeg_avcodec_receive_frame(pCodecCtx, s->pImgFrame);
    if (err == AVERROR(EINVAL))
    {
        K_CTX_LOG_DEBUG("err: {i}, AVERROR(EINVAL)", err);
        goto cleanup;
    }

    s->pSwsCtx = ffmpeg_sws_getContext(
        s->pImgFrame->width, s->pImgFrame->height, s->pImgFrame->format,
        s->pImgFrame->width, s->pImgFrame->height, AV_PIX_FMT_RGB24,
        SWS_LANCZOS,
        NULL, NULL, NULL
    );

    s->pConverted = ffmpeg_av_frame_alloc();
    ffmpeg_av_image_fill_linesizes(s->pConverted->linesize, AV_PIX_FMT_RGB24, s->pImgFrame->width);

    s->pConverted->format = AV_PIX_FMT_RGB24;
    s->pConverted->width = s->pImgFrame->width;
    s->pConverted->height = s->pImgFrame->height;

    K_CTX_LOG_DEBUG("format: {i}, converted format: {i}", s->pImgFrame->format, s->pConverted->format);

    err = ffmpeg_av_frame_get_buffer(s->pConverted, 0);
    if (err != 0)
    {
        K_CTX_LOG_DEBUG("av_frame_get_buffer()");
        goto cleanup;
    }

    ffmpeg_sws_scale_frame(s->pSwsCtx, s->pConverted, s->pImgFrame);

    s->coverImg = (Image){
        .pBuff = s->pConverted->data[0],
        .width = s->pConverted->width,
        .height = s->pConverted->height,
        .eFormat = ffmpeg_convertFormat(s->pConverted->format)
    };

cleanup:
    ffmpeg_avcodec_free_context(&pCodecCtx);
}

static IMAGE_PIXEL_FORMAT
ffmpeg_convertFormat(int format)
{
    switch (format)
    {
        default: return IMAGE_PIXEL_FORMAT_NONE;
        case AV_PIX_FMT_RGB24: return IMAGE_PIXEL_FORMAT_RGB8;
        case AV_PIX_FMT_RGBA: return IMAGE_PIXEL_FORMAT_RGBA8_UNASSOCIATED;
    }
}

#endif

bool
ffmpeg_DecoderInit(ffmpeg_Decoder* s)
{
    *s = (ffmpeg_Decoder){0};

    if (!ffmpeg_loadLibs()) return false;
    if (!k_MutexInit(&s->mtx, K_MUTEX_TYPE_PLAIN))
    {
        ffmpeg_unloadLibs();
        return false;
    }

    return true;
}

void
ffmpeg_DecoderDestroy(ffmpeg_Decoder* s)
{
    if (!s || !s->pStream) return; /* WARN: covers 1 simple quit case. */

    k_MutexLock(&s->mtx);
    ffmpeg_DecoderClose(s);
    k_MutexUnlock(&s->mtx);

    k_MutexDestroy(&s->mtx);
    ffmpeg_unloadLibs();

    K_CTX_LOG_DEBUG("DecoderDestroy()");
}

FFMPEG_ERROR
ffmpeg_DecoderWriteToRing(
    ffmpeg_Decoder* s,
    audio_RingBuffer* pRing,
    int nChannels,
    ssize_t* pNSamplesWritten,
    ssize_t* pPcmPos
)
{
    if (!s->pStream) return FFMPEG_ERROR_END_OF_FILE;

    int err = 0;
    ssize_t nWrites = 0;

    *pNSamplesWritten = 0;

    AVPacket* pPacket = s->pTmpPacket;
    while (ffmpeg_av_read_frame(s->pFormatCtx, pPacket) == 0)
    {
        if (pPacket->stream_index != s->pStream->index)
            goto readFrameLoopDone;

        err = ffmpeg_avcodec_send_packet(s->pCodecCtx, pPacket);
        if (err != 0 && err != AVERROR(EAGAIN))
            K_CTX_LOG_DEBUG("!EAGAIN");

        AVFrame* pFrame = s->pTmpFrame;
        while ((err = ffmpeg_avcodec_receive_frame(s->pCodecCtx, pFrame)) == 0)
        {
            double currentTimeInSeconds = av_q2d(s->pStream->time_base) * (pFrame->best_effort_timestamp + pFrame->nb_samples);
            s->currentMS = currentTimeInSeconds * 1000.0;
            ssize_t pcmPos = currentTimeInSeconds * pFrame->ch_layout.nb_channels * pFrame->sample_rate;
            s->currentSamplePos = pcmPos;
            *pPcmPos = pcmPos;

            /* NOTE: not changing sample rate here, but on the mixer side instead. */
            AVFrame* pRes = s->pCvtFrame;
            pRes->sample_rate = pFrame->sample_rate;
            pRes->ch_layout = pFrame->ch_layout;
            pRes->format = AV_SAMPLE_FMT_FLT;

            ffmpeg_swr_config_frame(s->pSwr, pRes, pFrame);
            err = ffmpeg_swr_convert_frame(s->pSwr, pRes, pFrame);

            if (err < 0)
            {
                char aBuff[AV_ERROR_MAX_STRING_SIZE] = {0};
                const int n = ffmpeg_av_strerror(err, aBuff, sizeof(aBuff));
                K_CTX_LOG_DEBUG("swr_convert_frame(): {:{i}:nts}", n, aBuff);
                goto loopDone;
            }

            const int nFrameSamples = pRes->nb_samples * pRes->ch_layout.nb_channels;
            ssize_t ringSize = audio_RingBufferPush(pRing, (float*)pRes->data[0], nFrameSamples);
            nWrites += nFrameSamples;

            /* Keep pushing until we hit high threshold, only then return. */
            if (ringSize >= audio_RING_BUFFER_HIGH_THRESHOLD)
            {
                *pNSamplesWritten = nWrites;
                ffmpeg_av_frame_unref(pFrame);
                ffmpeg_av_frame_unref(pRes);
                ffmpeg_av_packet_unref(pPacket);
                return FFMPEG_ERROR_OK;
            }

loopDone:
            ffmpeg_av_frame_unref(pFrame);
            ffmpeg_av_frame_unref(pRes);
        }

readFrameLoopDone:
        ffmpeg_av_packet_unref(pPacket);
    }

    return FFMPEG_ERROR_END_OF_FILE;
}

uint32_t
ffmpeg_DecoderGetSampleRate(ffmpeg_Decoder* s)
{
    return s->pStream ? s->pStream->codecpar->sample_rate : 0;
}

void
ffmpeg_DecoderSeekMS(ffmpeg_Decoder* s, double ms)
{
    if (!s->pCodecCtx) return;

    ffmpeg_avcodec_flush_buffers(s->pCodecCtx);
    int64_t pts = ffmpeg_av_rescale_q((double)ms / 1000.0 * AV_TIME_BASE, AV_TIME_BASE_Q, s->pStream->time_base);
    ffmpeg_av_seek_frame(s->pFormatCtx, s->audioStreamIdx, pts, 0);
}

int64_t
ffmpeg_DecoderGetCurrentSamplePos(ffmpeg_Decoder* s)
{
    return s->currentSamplePos;
}

int64_t
ffmpeg_DecoderGetCurrentMS(ffmpeg_Decoder* s)
{
    return s->currentMS;
}

int64_t
ffmpeg_DecoderGetTotalMS(ffmpeg_Decoder* s)
{
    int64_t totalCount = ffmpeg_DecoderGetTotalSamplesCount(s);
    uint32_t sr = ffmpeg_DecoderGetSampleRate(s);
    int nChannels = ffmpeg_DecoderGetChannelsCount(s);

    if (sr == 0 || nChannels == 0) return 0;
    else return (totalCount / sr / nChannels) * 1000;
}

K_NO_DISCARD int64_t
ffmpeg_DecoderGetTotalSamplesCount(ffmpeg_Decoder* s)
{
    if (!s->pFormatCtx || !s->pStream) return 0;

    int64_t res = (s->pFormatCtx->duration / (double)AV_TIME_BASE) * s->pStream->codecpar->sample_rate * s->pStream->codecpar->ch_layout.nb_channels;
    return res;
}

int
ffmpeg_DecoderGetChannelsCount(ffmpeg_Decoder* s)
{
    return s->pStream ? s->pStream->codecpar->ch_layout.nb_channels : 0;
}

k_StringView
ffmpeg_DecoderGetMetadata(ffmpeg_Decoder* s, const k_StringView svKey)
{
    if (!s->pStream) return (k_StringView){0};

    char aBuff[64] = {0};
    strncpy(aBuff, svKey.pData, K_MIN(svKey.size, K_ASIZE(aBuff) - 1));

    AVDictionaryEntry* pTag = NULL;
    pTag = ffmpeg_av_dict_get(s->pStream->metadata, aBuff, pTag, AV_DICT_IGNORE_SUFFIX);

    if (pTag)
    {
        return K_NTS(pTag->value);
    }
    else
    {
        pTag = ffmpeg_av_dict_get(s->pFormatCtx->metadata, aBuff, pTag, AV_DICT_IGNORE_SUFFIX);
        if (pTag) return K_NTS(pTag->value);
        else return (k_StringView){0};
    }
}

FFMPEG_ERROR
ffmpeg_DecoderOpen(ffmpeg_Decoder* s, k_StringView svPath)
{
    k_String sPathNullTerm = k_StringCreateSv(&k_GpaInst()->base, svPath); /* with null char */
    FFMPEG_ERROR eRet = FFMPEG_ERROR_OK;

    ffmpeg_av_log_set_level(AV_LOG_QUIET);

    int err = 0;

    err = ffmpeg_avformat_open_input(&s->pFormatCtx, k_StringData(&sPathNullTerm), NULL, NULL);
    if (err != 0)
    {
        eRet = FFMPEG_ERROR_FAIL;
        goto done;
    }

    err = ffmpeg_avformat_find_stream_info(s->pFormatCtx, NULL);
    if (err != 0)
    {
        eRet = FFMPEG_ERROR_FAIL;
        goto done;
    }

    int idx = ffmpeg_av_find_best_stream(s->pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (idx < 0)
    {
        eRet = FFMPEG_ERROR_FAIL;
        goto done;
    }

    s->audioStreamIdx = idx;
    s->pStream = s->pFormatCtx->streams[idx];

    const AVCodec* pCodec = ffmpeg_avcodec_find_decoder(s->pStream->codecpar->codec_id);
    if (!pCodec)
    {
        eRet = FFMPEG_ERROR_FAIL;
        goto done;
    }

    s->pCodecCtx = ffmpeg_avcodec_alloc_context3(pCodec);
    if (!s->pCodecCtx)
    {
        eRet = FFMPEG_ERROR_FAIL;
        goto done;
    }

    ffmpeg_avcodec_parameters_to_context(s->pCodecCtx, s->pStream->codecpar);

    err = ffmpeg_avcodec_open2(s->pCodecCtx, pCodec, NULL);
    if (err < 0)
    {
        K_CTX_LOG_DEBUG("avcodec_open2");
        eRet = FFMPEG_ERROR_FAIL;
        goto done;
    }

    err = ffmpeg_swr_alloc_set_opts2(&s->pSwr,
        &s->pStream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, s->pStream->codecpar->sample_rate,
        &s->pStream->codecpar->ch_layout, s->pStream->codecpar->format, s->pStream->codecpar->sample_rate,
        0, NULL
    );

    K_CTX_LOG_DEBUG("codec name: '{nts}', ch_layout: {i}, bit_rate: {i64}, sample_rate: {i}",
        pCodec->long_name, s->pStream->codecpar->ch_layout.nb_channels, s->pStream->codecpar->bit_rate, s->pStream->codecpar->sample_rate
    );

    ffmpeg_DecoderGetAttachedPicture(s);

    s->pTmpPacket = ffmpeg_av_packet_alloc();
    s->pTmpFrame = ffmpeg_av_frame_alloc();
    s->pCvtFrame = ffmpeg_av_frame_alloc();

    eRet = FFMPEG_ERROR_OK;

done:
    k_StringDestroy(&sPathNullTerm, &k_GpaInst()->base);
    if (err < 0) ffmpeg_DecoderClose(s);
    return eRet;
}

void
ffmpeg_DecoderClose(ffmpeg_Decoder* s)
{
    if (s->pFormatCtx) ffmpeg_avformat_close_input(&s->pFormatCtx);
    if (s->pCodecCtx) ffmpeg_avcodec_free_context(&s->pCodecCtx);
    if (s->pSwr) ffmpeg_swr_free(&s->pSwr);
    if (s->pImgPacket) ffmpeg_av_packet_free(&s->pImgPacket);
    if (s->pImgFrame) ffmpeg_av_frame_free(&s->pImgFrame);

    if (s->pTmpPacket) ffmpeg_av_packet_free(&s->pTmpPacket);
    if (s->pTmpFrame) ffmpeg_av_frame_free(&s->pTmpFrame);
    if (s->pCvtFrame) ffmpeg_av_frame_free(&s->pCvtFrame);

#ifdef OPT_CHAFA
    if (s->pConverted) ffmpeg_av_frame_free(&s->pConverted);
    if (s->pSwsCtx) ffmpeg_sws_freeContext(s->pSwsCtx), s->pSwsCtx = NULL;
#endif

    s->pStream = NULL;
    s->audioStreamIdx = 0;
    s->currentSamplePos = 0;
    s->currentMS = 0.0;
    s->coverImg = (Image){0};

    /* WARN: don't zero out s->mtx! */

    K_CTX_LOG_DEBUG("close()");
}

#ifdef OPT_CHAFA

Image
ffmpeg_DecoderGetCoverImage(ffmpeg_Decoder* s)
{
    return s->coverImg;
}

#endif

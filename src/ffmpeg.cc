#include "ffmpeg.hh"

#include "adt/OsAllocator.hh"
#include "adt/defer.hh"
#include "adt/logs.hh"
#include "adt/types.hh"
#include "adt/utils.hh"

extern "C"
{

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#ifdef USE_CHAFA
#include <libswscale/swscale.h>
#endif

}

namespace ffmpeg
{

[[maybe_unused]] static IMAGE_PIXEL_FORMAT
covertFormat(int format)
{
    switch (format)
    {
        default: return IMAGE_PIXEL_FORMAT::NONE;
        case AV_PIX_FMT_RGB24: return IMAGE_PIXEL_FORMAT::RGB8;
        case AV_PIX_FMT_RGBA: return IMAGE_PIXEL_FORMAT::RGBA8_UNASSOCIATED;
    }
};

struct Decoder
{
    AVStream* pStream {};
    AVFormatContext* pFormatCtx {};
    AVCodecContext* pCodecCtx {};
    SwrContext* pSwr {};
    int audioStreamIdx {};
    u64 currentSamplePos {};

    AVPacket* pImgPacket {};
    AVFrame* pImgFrame {};
    Opt<Image> oCoverImg {};

#ifdef USE_CHAFA
    SwsContext* pSwsCtx {};
    AVFrame* pConverted {};
#endif
};

Decoder*
DecoderAlloc(IAllocator* pAlloc)
{
    Decoder* s = (Decoder*)pAlloc->zalloc(1, sizeof(Decoder));
    return s;
}

void
DecoderClose(Decoder* s)
{
    if (s->pFormatCtx) avformat_close_input(&s->pFormatCtx);
    if (s->pCodecCtx) avcodec_free_context(&s->pCodecCtx);
    if (s->pSwr) swr_free(&s->pSwr);
    if (s->pImgPacket) av_packet_free(&s->pImgPacket);
    if (s->pImgFrame) av_frame_free(&s->pImgFrame);

#ifdef USE_CHAFA
    if (s->pConverted) av_frame_free(&s->pConverted);
    if (s->pSwsCtx) sws_freeContext(s->pSwsCtx);
#endif

    LOG_NOTIFY("DecoderClose()\n");

    *s = {};
}

Opt<String>
DecoderGetMetadataValue(Decoder* s, const String sKey)
{
    char aBuff[64] {};
    strncpy(aBuff, sKey.data(), utils::min(u64(sKey.getSize()), utils::size(aBuff) - 1));

    AVDictionaryEntry* pTag {};
    pTag = av_dict_get(s->pStream->metadata, aBuff, pTag, AV_DICT_IGNORE_SUFFIX);

    if (pTag) return {pTag->value};
    else
    {
        pTag = av_dict_get(s->pFormatCtx->metadata, aBuff, pTag, AV_DICT_IGNORE_SUFFIX);
        if (pTag) return {pTag->value};
        else return {};
    }
}

#ifdef USE_CHAFA
static void
DecoderGetAttachedPicture(Decoder* s)
{
    int err = 0;
    AVStream* pStream {};

    for (u32 i = 0; i < s->pFormatCtx->nb_streams; ++i)
    {
        auto* itStream = s->pFormatCtx->streams[i];
        if (itStream->disposition & AV_DISPOSITION_ATTACHED_PIC)
        {
            LOG_WARN("Found 'attached_pic'\n");
            pStream = itStream;
            break;
        }
    }

    if (!pStream) return;

    const AVCodec* pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
    if (!pCodec) return;

    auto* pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) return;
    defer( avcodec_free_context(&pCodecCtx) );

    avcodec_parameters_to_context(pCodecCtx, pStream->codecpar);
    err = avcodec_open2(pCodecCtx, pCodec, {});
    if (err < 0)
    {
        LOG("avcodec_open2()\n");
        return;
    }

    LOG_WARN("codec name: '{}'\n", pCodec->long_name);

    s->pImgPacket = av_packet_alloc();
    err = av_read_frame(s->pFormatCtx, s->pImgPacket);
    if (err != 0) return;

    err = avcodec_send_packet(pCodecCtx, s->pImgPacket);
    if (err != 0) return;

    s->pImgFrame = av_frame_alloc();
    err = avcodec_receive_frame(pCodecCtx, s->pImgFrame);
    if (err == AVERROR(EINVAL))
        LOG_BAD("err: {}, AVERROR(EINVAL)\n", err);

    s->pSwsCtx = sws_getContext(
        s->pImgFrame->width, s->pImgFrame->height, (AVPixelFormat)s->pImgFrame->format,
        s->pImgFrame->width, s->pImgFrame->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, {}, {}, {}
    );

    s->pConverted = av_frame_alloc();
    s->pConverted->format = AV_PIX_FMT_RGB24;
    s->pConverted->width = s->pImgFrame->width;
    s->pConverted->height = s->pImgFrame->height;

    err = av_frame_get_buffer(s->pConverted, 0);
    if (err != 0) { LOG_BAD("av_frame_get_buffer()\n"); return; }

    sws_scale(s->pSwsCtx,
        s->pImgFrame->data, s->pImgFrame->linesize, 0, s->pImgFrame->height,
        s->pConverted->data, s->pConverted->linesize
    );

    LOG_WARN("width: {}, height: {}, format: {}\n", s->pImgFrame->width, s->pImgFrame->height, s->pImgFrame->format);

    s->oCoverImg = Image {
        .pBuff = s->pConverted->data[0],
        .width = u32(s->pConverted->width),
        .height = u32(s->pConverted->height),
        .eFormat = covertFormat(s->pConverted->format)
    };
}
#else
    #define DecoderGetAttachedPicture(...) (void)0
#endif

Opt<Image>
DecoderGetCoverImage(Decoder* s)
{
    return s->oCoverImg;
}

audio::ERROR
DecoderOpen(Decoder* s, String sPath)
{
    String sPathNullTerm = StringAlloc(OsAllocatorGet(), sPath); /* with null char */
    defer( sPathNullTerm.destroy(OsAllocatorGet()) );

    int err = 0;
    defer( if (err < 0) DecoderClose(s) );

    err = avformat_open_input(&s->pFormatCtx, sPathNullTerm.data(), {}, {});
    if (err != 0) return audio::ERROR::FAIL;

    err = avformat_find_stream_info(s->pFormatCtx, {});
    if (err != 0) return audio::ERROR::FAIL;

    int idx = av_find_best_stream(s->pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, {}, 0);
    if (idx < 0) return audio::ERROR::FAIL;

    s->audioStreamIdx = idx;
    s->pStream = s->pFormatCtx->streams[idx];

    const AVCodec* pCodec = avcodec_find_decoder(s->pStream->codecpar->codec_id);
    if (!pCodec) return audio::ERROR::FAIL;

    s->pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!s->pCodecCtx) return audio::ERROR::FAIL;

    avcodec_parameters_to_context(s->pCodecCtx, s->pStream->codecpar);

    err = avcodec_open2(s->pCodecCtx, pCodec, {});
    if (err < 0)
    {
        LOG("avcodec_open2\n");
        return audio::ERROR::FAIL;
    }

    err = swr_alloc_set_opts2(&s->pSwr,
        &s->pStream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, s->pStream->codecpar->sample_rate,
        &s->pStream->codecpar->ch_layout, (AVSampleFormat)s->pStream->codecpar->format, s->pStream->codecpar->sample_rate,
        0, {}
    );

    LOG_NOTIFY("codec name: '{}'\n", pCodec->long_name);

    DecoderGetAttachedPicture(s);

    return audio::ERROR::OK_;
}

audio::ERROR
DecoderWriteToBuffer(
    Decoder* s,
    f32* pBuff,
    const u32 buffSize,
    const u32 nFrames,
    const u32 nChannles,
    long* pSamplesWritten,
    u64* pPcmPos
)
{
    int err = 0;
    long nWrites = 0;

    *pSamplesWritten = 0;

    AVPacket packet {};
    while (av_read_frame(s->pFormatCtx, &packet) == 0)
    {
        defer( av_packet_unref(&packet) );

        if (packet.stream_index != s->pStream->index) continue;
        err = avcodec_send_packet(s->pCodecCtx, &packet);
        if (err != 0 && err != AVERROR(EAGAIN))
            LOG_WARN("!EAGAIN\n");

        AVFrame frame {};
        while ((err = avcodec_receive_frame(s->pCodecCtx, &frame)) == 0)
        {
            defer( av_frame_unref(&frame) );

            f64 currentTimeInSeconds = av_q2d(s->pStream->time_base) * (frame.best_effort_timestamp + frame.nb_samples);
            long pcmPos = currentTimeInSeconds * frame.ch_layout.nb_channels * frame.sample_rate;
            s->currentSamplePos = pcmPos;
            *pPcmPos = pcmPos;

            AVFrame res {};
            /* NOTE: not changing sample rate here, changing pipewire's sample rate instead */
            res.sample_rate = frame.sample_rate;
            res.ch_layout = frame.ch_layout;
            res.format = AV_SAMPLE_FMT_FLT;
            defer( av_frame_unref(&res) );

            swr_config_frame(s->pSwr, &res, &frame);
            err = swr_convert_frame(s->pSwr, &res, &frame);

            if (err < 0)
            {
                char buff[16] {};
                av_strerror(err, buff, utils::size(buff));
                LOG_WARN("swr_convert_frame(): {}\n", buff);
                continue;
            }

            u32 maxSamples = res.nb_samples * res.ch_layout.nb_channels;
            if (maxSamples >= buffSize) maxSamples = buffSize - 1;

            const auto& nFrameChannles = res.ch_layout.nb_channels;
            assert(nFrameChannles > 0);
            if (nFrameChannles == 2)
            {
                utils::copy(pBuff + nWrites, (f32*)(res.data[0]), maxSamples);
                nWrites += maxSamples;
            }
            else if (nFrameChannles == 1)
            {
                f32* data = (f32*)(res.data[0]);
                for (u32 i = 0; i < maxSamples; ++i)
                {
                    for (u32 j = 0; j < nChannles; ++j)
                        pBuff[nWrites++] = data[i];
                }
            }
            else
            {
                f32* data = (f32*)(res.data[0]);
                for (u32 i = 0; i < maxSamples; i += nFrameChannles)
                {
                    for (u32 j = 0; j < nChannles; ++j)
                        pBuff[nWrites++] = data[i + j];
                }
            }

            if (nWrites >= maxSamples && nWrites >= nFrames*2) /* mul by nChannels */
            {
                *pSamplesWritten += nWrites;
                return audio::ERROR::OK_;
            }
        }
    }

    return audio::ERROR::END_OF_FILE;
}

u32
DecoderGetSampleRate(Decoder* s)
{
    return s->pStream->codecpar->sample_rate;
}

static void
DecoderSeekFrame(Decoder* s, u64 frame)
{
    avcodec_flush_buffers(s->pCodecCtx);
    avformat_seek_file(s->pFormatCtx, s->pStream->index, 0, frame, frame, AVSEEK_FLAG_BACKWARD);
}

void
DecoderSeekMS(Decoder* s, long ms)
{
    auto frameNumber = av_rescale(ms, s->pStream->time_base.den, s->pStream->time_base.num);
    frameNumber /= 1000;
    DecoderSeekFrame(s, frameNumber);
}

long
DecoderGetCurrentSamplePos(Decoder* s)
{
    return s->currentSamplePos;
}

long
DecoderGetTotalSamplesCount(Decoder* s)
{
    long res = (s->pFormatCtx->duration / (f32)AV_TIME_BASE) * s->pStream->codecpar->sample_rate * s->pStream->codecpar->ch_layout.nb_channels;
    return res;
}

int
DecoderGetChannelsCount(Decoder* s)
{
    return s->pStream->codecpar->ch_layout.nb_channels;
}

Reader::Reader(IAllocator* pAlloc) : super {&inl_ReaderVTable}, m_pDecoder {DecoderAlloc(pAlloc)} {}

} /* namespace ffmpeg */

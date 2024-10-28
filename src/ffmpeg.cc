#include "ffmpeg.hh"

#include "adt/defer.hh"
#include "adt/types.hh"
#include "adt/utils.hh"
#include "adt/OsAllocator.hh"
#include "logs.hh"

extern "C"
{

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

}

namespace ffmpeg
{

struct Decoder
{
    String path {};
    AVStream* pStream {};
    AVFormatContext* pFormatCtx {};
    AVCodecContext* pCodecCtx {};
    SwrContext* pSwr {};
    int audioStreamIdx {};
    u32 lastFrameSampleRate {};
};

Decoder*
DecoderAlloc(Allocator* pAlloc)
{
    Decoder* s = (Decoder*)alloc(pAlloc, 1, sizeof(Decoder));
    if (!s) return nullptr;

    *s = {};
    return s;
}

void
DecoderClose(Decoder* s)
{
    /*avformat_free_context(s->pFormatCtx);*/
    if (s->pFormatCtx) avformat_close_input(&s->pFormatCtx);
    if (s->pCodecCtx) avcodec_free_context(&s->pCodecCtx);
    if (s->pSwr) swr_free(&s->pSwr);
    /*avcodec_close(s->pCodecCtx);*/
    LOG_NOTIFY("DecoderClose()\n");
}

ERROR
DecoderOpen(Decoder* s, String sPath)
{
    String sPathNullTerm = StringAlloc(&inl_OsAllocator.base, sPath); /* with null char */
    defer( StringDestroy(&inl_OsAllocator.base, &sPathNullTerm) );

    int err = 0;
    defer( if (err < 0) DecoderClose(s) );

    err = avformat_open_input(&s->pFormatCtx, sPathNullTerm.pData, {}, {});
    if (err != 0) return ERROR::FILE_OPENING;

    err = avformat_find_stream_info(s->pFormatCtx, {});
    if (err != 0) return ERROR::AUDIO_STREAM_NOT_FOUND;

    LOG("nb_streams: {}\n", s->pFormatCtx->nb_streams);

    int idx = av_find_best_stream(s->pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, {}, 0);
    if (idx < 0) return ERROR::AUDIO_STREAM_NOT_FOUND;

    s->audioStreamIdx = idx;
    s->pStream = s->pFormatCtx->streams[idx];

    const AVCodec* pCodec = avcodec_find_decoder(s->pStream->codecpar->codec_id);

    if (!pCodec) return ERROR::DECODER_NOT_FOUND;

    s->pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!s->pCodecCtx) return ERROR::DECODING_CONTEXT_ALLOCATION;

    avcodec_parameters_to_context(s->pCodecCtx, s->pStream->codecpar);

    err = avcodec_open2(s->pCodecCtx, pCodec, {});
    if (err < 0)
    {
        LOG("avcodec_open2\n");
        return ERROR::UNKNOWN;
    }

    err = swr_alloc_set_opts2(&s->pSwr,
        &s->pStream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, 48000,
        &s->pStream->codecpar->ch_layout, (AVSampleFormat)s->pStream->codecpar->format, s->pStream->codecpar->sample_rate,
        0, {}
    );

    LOG_NOTIFY("codec name: '{}'\n", pCodec->long_name);

    return ERROR::OK;
}

ERROR
DecoderWriteToBuffer(
    Decoder* s,
    f32* pBuff,
    const u32 buffSize,
    const u32 nFrames,
    const u32 nChannles,
    long* pSamplesWritten
)
{
    int err = 0;
    long nWrites = 0;

    *pSamplesWritten = 0;

    AVPacket packet {};
    while (av_read_frame(s->pFormatCtx, &packet) == 0)
    {
        if (packet.stream_index != s->pStream->index) continue;
        err = avcodec_send_packet(s->pCodecCtx, &packet);
        if (err != 0 && err != AVERROR(EAGAIN))
            LOG_WARN("!EAGAIN\n");

        defer( av_packet_unref(&packet) );

        AVFrame frame {};
        while ((err = avcodec_receive_frame(s->pCodecCtx, &frame)) == 0)
        {
            defer( av_frame_unref(&frame) );

            AVFrame res {};
            /* NOTE: not changing sample rate here, chaning pipewire's sample rate instead */
            res.sample_rate = frame.sample_rate;
            res.ch_layout = frame.ch_layout;
            res.format = AV_SAMPLE_FMT_FLT;
            defer( av_frame_unref(&res) );

            s->lastFrameSampleRate = res.sample_rate;

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
                auto* data = (f32*)(res.data[0]);
                for (u32 i = 0; i < maxSamples; ++i)
                {
                    for (u32 j = 0; j < nChannles; ++j)
                        pBuff[nWrites++] = data[i];
                }
            }
            else
            {
                auto* data = (f32*)(res.data[0]);
                for (u32 i = 0; i < maxSamples; i += nFrameChannles)
                {
                    for (u32 j = 0; j < nChannles; ++j)
                        pBuff[nWrites++] = data[i + j];
                }
            }

            /* when resampling diffirent sample rates there might be leftovers */
            /*auto delay = swr_get_delay(s->pSwr, frame.sample_rate);*/

            // if (delay > 0)
            // {
            //     AVFrame res2 {};
            //     res2.sample_rate = 48000;
            //     res2.ch_layout = frame.ch_layout;
            //     res2.format = AV_SAMPLE_FMT_FLT;
            //     err = swr_convert_frame(s->pSwr, &res2, {});

            //     u32 maxSamples2 = res2.nb_samples * res2.ch_layout.nb_channels;
            //     LOG("maxSamples2: {}\n", maxSamples2);

            //     utils::copy(pBuff + nWrites, (f32*)(res2.data[0]), maxSamples2);
            //     nWrites += maxSamples2;
            //     av_frame_unref(&res2);
            // }

#if 0
            /* non-resampled frame handling */
            u32 maxSamples = frame.nb_samples * frame.ch_layout.nb_channels;
            for (int i = 0; i < frame.nb_samples; ++i)
            {
                for (int ch = 0; ch < frame.ch_layout.nb_channels && ch < 2; ++ch)
                {
                    f32 s = ((f32*)frame.data[ch])[i];
                    pBuff[nWrites++] = s;
                }
            }
#endif

            if (nWrites >= maxSamples && nWrites >= nFrames*2) /* mul by nChannels */
            {
                *pSamplesWritten += nWrites;
                return ERROR::OK;
            }
        }
    }

    return ERROR::EOF_;
}

u32
DecoderGetSampleRate(Decoder* s)
{
    LOG("sample_rate: {}\n", s->pStream->codecpar->sample_rate);
    return s->pStream->codecpar->sample_rate;
}

} /* namespace ffmpeg */

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
    int audioStreamIdx {};
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
    /*avcodec_close(s->pCodecCtx);*/
    LOG_NOTIFY("DecoderClose()\n");
}

int
printError(const char* pPrefix, int errorCode)
{
    if (errorCode == 0)
    {
        return 0;
    }
    else
    {
        char buf[64] {};

        if (av_strerror(errorCode, buf, utils::size(buf)) != 0)
            strcpy(buf, "UNKNOWN_ERROR");

        CERR("{} ({}: {})\n", pPrefix, errorCode, buf);

        return errorCode;
    }
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

    LOG_NOTIFY("codec name: '{}'\n", pCodec->long_name);

    s->pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!s->pCodecCtx) return ERROR::DECODING_CONTEXT_ALLOCATION;

    avcodec_parameters_to_context(s->pCodecCtx, s->pStream->codecpar);

    err = avcodec_open2(s->pCodecCtx, pCodec, {});
    if (err < 0)
    {
        LOG("avcodec_open2\n");
        return ERROR::UNKNOWN;
    }

    return ERROR::OK;
}

ERROR
DecoderWriteToBuffer(Decoder* s, f32* pBuff, u32 buffSie, u32 nFrames, long* pSamplesWritten)
{
    int err = 0;
    long nWrites = 0;

    /* NOTE: not sure why these have to be allocated. */

    *pSamplesWritten = 0;

    SwrContext* pSwr {};
    err = swr_alloc_set_opts2(&pSwr,
        &s->pStream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, 48000,
        &s->pStream->codecpar->ch_layout, (AVSampleFormat)s->pStream->codecpar->format, s->pStream->codecpar->sample_rate,
        0, {}
    );
    defer( swr_free(&pSwr) );

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
            defer( av_frame_unref(&res) );

            /* force these settings for pipewire */
            res.sample_rate = 48000;
            res.ch_layout = frame.ch_layout;
            res.format = AV_SAMPLE_FMT_FLT;

            err = swr_convert_frame(pSwr, &res, &frame);
            if (err < 0)
            {
                char buff[16] {};
                av_strerror(err, buff, utils::size(buff));
                LOG_WARN("swr_convert_frame(): {}\n", buff);
                continue;
            }

            u32 maxSamples = res.nb_samples * res.ch_layout.nb_channels;
            utils::copy(pBuff + nWrites, (f32*)(res.data[0]), maxSamples);
            nWrites += maxSamples;

#if 0
            /* non-resampled frame handling */
            u32 maxSamples = frame.nb_samples * frame.ch_layout.nb_channels;
            for (int i = 0; i < frame.nb_samples; ++i)
            {
                for (int ch = 0; ch < frame.ch_layout.nb_channels; ++ch)
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

} /* namespace ffmpeg */

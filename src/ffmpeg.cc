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
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>

}

/* https://gist.github.com/jcelerier/c28310e34c189d37e99609be8cd74bdcG*/
#define RAW_OUT_ON_PLANAR true

namespace ffmpeg
{

struct Decoder
{
    String path {};
    AVStream* pStream {};
    AVFormatContext* pFormatCtx {};
    AVCodecContext* pCodecCtx {};
    /*AVFrame* pFrame {};*/
    /*AVPacket* pPacket {};*/
    SwrContext* pSwr {};
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
    /* TODO: cleanup... */

    if (s->pFormatCtx) avformat_close_input(&s->pFormatCtx);
    if (s->pCodecCtx) avcodec_free_context(&s->pCodecCtx);
    /*if (s->pFrame) av_frame_free(&s->pFrame);*/
    /*if (s->pPacket) av_packet_free(&s->pPacket);*/

    *s = {};
}

/*static FILE* s_outFile;*/

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

void
printStreamInformation(const AVCodec* pCodec, const AVCodecContext* pCodecCtx, int audioStreamIndex)
{
#ifndef NDEBUG

    LOG("Codec: {}\n", pCodec->long_name);
    if (pCodec->sample_fmts != nullptr)
    {
        LOG("Supported sample formats: ");
        for (int i = 0; pCodec->sample_fmts[i] != -1; ++i)
        {
            CERR("{}", av_get_sample_fmt_name(pCodec->sample_fmts[i]));
            if (pCodec->sample_fmts[i + 1] != -1)
                CERR(", ");
        }
        CERR("\n");
    }
    CERR("---------\n");
    CERR("Stream:        {}\n", audioStreamIndex);
    CERR("Sample Format: {}\n", av_get_sample_fmt_name(pCodecCtx->sample_fmt));
    CERR("Sample Rate:   {}\n", pCodecCtx->sample_rate);
    CERR("Sample Size:   {}\n", av_get_bytes_per_sample(pCodecCtx->sample_fmt));
    CERR("Channels:      {}\n", pCodecCtx->ch_layout.nb_channels);
    CERR("Planar:        {}\n", av_sample_fmt_is_planar(pCodecCtx->sample_fmt));
    CERR(
        "Float Output:  {}\n",
        !RAW_OUT_ON_PLANAR || av_sample_fmt_is_planar(pCodecCtx->sample_fmt) ? "yes" : "no"
    );

#endif
}

// void
// drainDecoder(AVCodecContext* pCodecCtx, AVFrame* pFrame, Buffer* pBW)
// {
//     int err = 0;
//     /* Some codecs may buffer frames. Sending nullptr activates drain-mode. */
//     if ((err = avcodec_send_packet(pCodecCtx, nullptr)) == 0)
//     {
//         /* Read the remaining packets from the decoder. */
//         err = receiveAndHandle(pCodecCtx, pFrame, pBW);
//         if (err != AVERROR(EAGAIN) && err != AVERROR_EOF)
//         {
//             /* Neither EAGAIN nor EOF => Something went wrong. */
//             printError("Receive error.", err);
//         }
//     }
//     else
//     {
//         /* Something went wrong. */
//         printError("Send error.", err);
//     }
// }

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

    /*s->pPacket = av_packet_alloc();*/
    /*s->pFrame = av_frame_alloc();*/

    err = swr_alloc_set_opts2(&s->pSwr,
        &s->pStream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, 48000,
        &s->pStream->codecpar->ch_layout, (AVSampleFormat)s->pStream->codecpar->format, s->pStream->codecpar->sample_rate,
        0, {}
    );

    return ERROR::OK;
}

ERROR
DecoderWriteToBuffer(Decoder* s, f32* pBuff, u32 buffSie, u32 nFrames, long* pSamplesWritten)
{
    int err = 0;
    long nWrites = 0;

    /* NOTE: not sure why these have to be allocated. */
    AVFrame frame {};
    AVPacket packet {};

    *pSamplesWritten = 0;

    while (av_read_frame(s->pFormatCtx, &packet) == 0)
    {
        if (packet.stream_index != s->pStream->index) continue;
        err = avcodec_send_packet(s->pCodecCtx, &packet);
        if (err != 0 && err != AVERROR(EAGAIN))
            LOG_WARN("!EAGAIN\n");

        while ((err = avcodec_receive_frame(s->pCodecCtx, &frame)) == 0)
        {
            defer( av_frame_unref(&frame) );

            /*AVFrame* pRes = av_frame_alloc();*/
            /*defer( av_frame_free(&pRes) );*/
            AVFrame res {};

            /* force these settings for pipewire */
            res.sample_rate = 48000;
            res.ch_layout = frame.ch_layout;
            res.format = AV_SAMPLE_FMT_FLT;

            err = swr_convert_frame(s->pSwr, &res, &frame);
            if (err != 0)
            {
                LOG_WARN("swr_convert_frame\n");
                return ERROR::UNKNOWN;
            }

            u32 maxSamples = res.nb_samples * res.ch_layout.nb_channels;
            memcpy(pBuff + nWrites, res.data[0], maxSamples * sizeof(f32));
            nWrites += maxSamples;

#if 0
            /* non-resampled frame handling */
            for (int i = 0; i < pRes->nb_samples; ++i)
            {
                for (int ch = 0; ch < pRes->ch_layout.nb_channels; ++ch)
                {
                    f32 s = ((f32*)pRes->data[ch])[i];
                    pBuff[nf++] = s;
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

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
    AVStream* pStream {};
    AVFormatContext* pFormatCtx {};
    AVCodecContext* pCodecCtx {};
    SwrContext* pSwr {};
    int audioStreamIdx {};
    u64 currentSamplePos {};
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

    *s = {};
}

Option<String>
DecoderGetMetadataValue(Decoder* s, const String sKey)
{
    char aBuff[64] {};
    strncpy(aBuff, sKey.pData, utils::min(u64(sKey.size), utils::size(aBuff) - 1));

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
        &s->pStream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, s->pStream->codecpar->sample_rate,
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
        if (packet.stream_index != s->pStream->index) continue;
        err = avcodec_send_packet(s->pCodecCtx, &packet);
        if (err != 0 && err != AVERROR(EAGAIN))
            LOG_WARN("!EAGAIN\n");

        defer( av_packet_unref(&packet) );

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

            if (nWrites >= maxSamples && nWrites >= nFrames*2) /* mul by nChannels */
            {
                *pSamplesWritten += nWrites;
                return ERROR::OK;
            }
        }
    }

    return ERROR::END_OF_FILE;
}

u32
DecoderGetSampleRate(Decoder* s)
{
    return s->pStream->codecpar->sample_rate;
}

static void
DecoderSeekFrame(Decoder* s, u64 frame)
{
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

} /* namespace ffmpeg */

#include "ffmpeg.hh"

#include "adt/OsAllocator.hh"
#include "adt/defer.hh"
#include "adt/logs.hh"
#include "adt/types.hh"
#include "adt/utils.hh"

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

void
Decoder::close()
{
    if (m_pFormatCtx) avformat_close_input(&m_pFormatCtx);
    if (m_pCodecCtx) avcodec_free_context(&m_pCodecCtx);
    if (m_pSwr) swr_free(&m_pSwr);
    if (m_pImgPacket) av_packet_free(&m_pImgPacket);
    if (m_pImgFrame) av_frame_free(&m_pImgFrame);

#ifdef USE_CHAFA
    if (m_pConverted) av_frame_free(&m_pConverted);
    if (m_pSwsCtx) sws_freeContext(m_pSwsCtx);
#endif

    LOG_NOTIFY("close()\n");

    *this = {};
}

s64
Decoder::getCurrentMS()
{
    return m_currentMS;
}

s64
Decoder::getTotalMS()
{
    s64 total = getTotalSamplesCount();
    f64 ret = (av_q2d(m_pStream->time_base) * total) / getChannelsCount() * 1000.0;

    return ret;
}


Opt<String>
Decoder::getMetadataValue(const String sKey)
{
    char aBuff[64] {};
    strncpy(aBuff, sKey.data(), utils::min(u64(sKey.getSize()), utils::size(aBuff) - 1));

    AVDictionaryEntry* pTag {};
    pTag = av_dict_get(m_pStream->metadata, aBuff, pTag, AV_DICT_IGNORE_SUFFIX);

    if (pTag) return {pTag->value};
    else
    {
        pTag = av_dict_get(m_pFormatCtx->metadata, aBuff, pTag, AV_DICT_IGNORE_SUFFIX);
        if (pTag) return {pTag->value};
        else return {};
    }
}

#ifdef USE_CHAFA
void
Decoder::getAttachedPicture()
{
    int err = 0;
    AVStream* pStream {};

    for (u32 i = 0; i < m_pFormatCtx->nb_streams; ++i)
    {
        auto* itStream = m_pFormatCtx->streams[i];
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

    m_pImgPacket = av_packet_alloc();
    err = av_read_frame(m_pFormatCtx, m_pImgPacket);
    if (err != 0) return;

    err = avcodec_send_packet(pCodecCtx, m_pImgPacket);
    if (err != 0) return;

    m_pImgFrame = av_frame_alloc();
    err = avcodec_receive_frame(pCodecCtx, m_pImgFrame);
    if (err == AVERROR(EINVAL))
        LOG_BAD("err: {}, AVERROR(EINVAL)\n", err);

    m_pSwsCtx = sws_getContext(
        m_pImgFrame->width, m_pImgFrame->height, (AVPixelFormat)m_pImgFrame->format,
        m_pImgFrame->width, m_pImgFrame->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, {}, {}, {}
    );

    m_pConverted = av_frame_alloc();
    m_pConverted->format = AV_PIX_FMT_RGB24;
    m_pConverted->width = m_pImgFrame->width;
    m_pConverted->height = m_pImgFrame->height;

    err = av_frame_get_buffer(m_pConverted, 0);
    if (err != 0) { LOG_BAD("av_frame_get_buffer()\n"); return; }

    sws_scale(m_pSwsCtx,
        m_pImgFrame->data, m_pImgFrame->linesize, 0, m_pImgFrame->height,
        m_pConverted->data, m_pConverted->linesize
    );

    LOG_WARN("width: {}, height: {}, format: {}\n", m_pImgFrame->width, m_pImgFrame->height, m_pImgFrame->format);

    m_oCoverImg = Image {
        .pBuff = m_pConverted->data[0],
        .width = u32(m_pConverted->width),
        .height = u32(m_pConverted->height),
        .eFormat = covertFormat(m_pConverted->format)
    };
}
#else
    #define getAttachedPicture(...) (void)0
#endif

Opt<Image>
Decoder::getCoverImage()
{
    return m_oCoverImg;
}

audio::ERROR
Decoder::open(String sPath)
{
    String sPathNullTerm = StringAlloc(OsAllocatorGet(), sPath); /* with null char */
    defer( sPathNullTerm.destroy(OsAllocatorGet()) );

    int err = 0;
    defer( if (err < 0) close() );

    err = avformat_open_input(&m_pFormatCtx, sPathNullTerm.data(), {}, {});
    if (err != 0) return audio::ERROR::FAIL;

    err = avformat_find_stream_info(m_pFormatCtx, {});
    if (err != 0) return audio::ERROR::FAIL;

    int idx = av_find_best_stream(m_pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, {}, 0);
    if (idx < 0) return audio::ERROR::FAIL;

    m_audioStreamIdx = idx;
    m_pStream = m_pFormatCtx->streams[idx];

    const AVCodec* pCodec = avcodec_find_decoder(m_pStream->codecpar->codec_id);
    if (!pCodec) return audio::ERROR::FAIL;

    m_pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!m_pCodecCtx) return audio::ERROR::FAIL;

    avcodec_parameters_to_context(m_pCodecCtx, m_pStream->codecpar);

    err = avcodec_open2(m_pCodecCtx, pCodec, {});
    if (err < 0)
    {
        LOG("avcodec_open2\n");
        return audio::ERROR::FAIL;
    }

    err = swr_alloc_set_opts2(&m_pSwr,
        &m_pStream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, m_pStream->codecpar->sample_rate,
        &m_pStream->codecpar->ch_layout, (AVSampleFormat)m_pStream->codecpar->format, m_pStream->codecpar->sample_rate,
        0, {}
    );

    LOG_NOTIFY("codec name: '{}', ch_layout: {}\n", pCodec->long_name, m_pStream->codecpar->ch_layout.nb_channels);

    getAttachedPicture();

    return audio::ERROR::OK_;
}

audio::ERROR
Decoder::writeToBuffer(
    f32* pBuff,
    const u32 buffSize,
    const u32 nFrames,
    [[maybe_unused]] const u32 nChannles,
    long* pSamplesWritten,
    s64* pPcmPos
)
{
    int err = 0;
    long nWrites = 0;

    *pSamplesWritten = 0;

    AVPacket packet {};
    while (av_read_frame(m_pFormatCtx, &packet) == 0)
    {
        defer( av_packet_unref(&packet) );

        if (packet.stream_index != m_pStream->index) continue;
        err = avcodec_send_packet(m_pCodecCtx, &packet);
        if (err != 0 && err != AVERROR(EAGAIN))
            LOG_WARN("!EAGAIN\n");

        AVFrame frame {};
        while ((err = avcodec_receive_frame(m_pCodecCtx, &frame)) == 0)
        {
            defer( av_frame_unref(&frame) );

            f64 currentTimeInSeconds = av_q2d(m_pStream->time_base) * (frame.best_effort_timestamp + frame.nb_samples);
            m_currentMS = currentTimeInSeconds * 1000.0;
            long pcmPos = currentTimeInSeconds * frame.ch_layout.nb_channels * frame.sample_rate;
            m_currentSamplePos = pcmPos;
            *pPcmPos = pcmPos;

            AVFrame res {};
            /* NOTE: not changing sample rate here, changing pipewire's sample rate instead */
            res.sample_rate = frame.sample_rate;
            res.ch_layout = frame.ch_layout;
            res.format = AV_SAMPLE_FMT_FLT;
            defer( av_frame_unref(&res) );

            swr_config_frame(m_pSwr, &res, &frame);
            err = swr_convert_frame(m_pSwr, &res, &frame);

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

            utils::copy(pBuff + nWrites, (f32*)(res.data[0]), maxSamples);
            nWrites += maxSamples;

            if (nWrites >= maxSamples && nWrites >= nFrames * nFrameChannles) /* mul by nChannels */
            {
                *pSamplesWritten += nWrites;
                return audio::ERROR::OK_;
            }
        }
    }

    return audio::ERROR::END_OF_FILE;
}

u32
Decoder::getSampleRate()
{
    return m_pStream->codecpar->sample_rate;
}

void
Decoder::seekMS(f64 ms)
{
	avcodec_flush_buffers(m_pCodecCtx);
	s64 pts = av_rescale_q(f64(ms) / 1000.0 * AV_TIME_BASE, AV_TIME_BASE_Q, m_pStream->time_base);
    av_seek_frame(m_pFormatCtx, m_audioStreamIdx, pts, 0);
}

s64
Decoder::getCurrentSamplePos()
{
    return m_currentSamplePos;
}

s64
Decoder::getTotalSamplesCount()
{
    s64 res = (m_pFormatCtx->duration / (f64)AV_TIME_BASE) * m_pStream->codecpar->sample_rate * m_pStream->codecpar->ch_layout.nb_channels;
    return res;
}

int
Decoder::getChannelsCount()
{
    return m_pStream->codecpar->ch_layout.nb_channels;
}

} /* namespace ffmpeg */

#include "Decoder.hh"

extern "C"
{

#include <libavutil/imgutils.h>

}

using namespace adt;

namespace platform::ffmpeg
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

#ifdef OPT_CHAFA
    if (m_pConverted) av_frame_free(&m_pConverted);
    if (m_pSwsCtx) sws_freeContext(m_pSwsCtx), m_pSwsCtx = {};
#endif

    m_pStream = {};
    m_audioStreamIdx = {};
    m_currentSamplePos = {};
    m_currentMS = {};
    m_coverImg = {};

    /* WARN: don't zero out m_mtx! */

    LogDebug("close()\n");
}

Decoder&
Decoder::init()
{
    new(&m_mtx) Mutex {Mutex::TYPE::PLAIN};
    return *this;
}

void
Decoder::destroy()
{
    LogDebug("Decoder::destroy() ATTEMPT\n");

    {
        LockGuard lockGuard {&m_mtx};
        close();
    }
    m_mtx.destroy();

    LogDebug("Decoder::destroy()\n");
}

i64
Decoder::getCurrentMS()
{
    return m_currentMS;
}

i64
Decoder::getTotalMS()
{
    i64 totalCount = getTotalSamplesCount();
    u32 sr = getSampleRate();
    int nChannels = getChannelsCount();

    if (sr == 0 || nChannels == 0) return 0;
    else return (totalCount / sr / nChannels) * 1000;
}


StringView
Decoder::getMetadata(const StringView svKey)
{
    if (!m_pStream) return {};

    char aBuff[64] {};
    strncpy(aBuff, svKey.data(), utils::min(svKey.size(), utils::size(aBuff) - 1));

    AVDictionaryEntry* pTag {};
    pTag = av_dict_get(m_pStream->metadata, aBuff, pTag, AV_DICT_IGNORE_SUFFIX);

    if (pTag)
    {
        return {pTag->value};
    }
    else
    {
        pTag = av_dict_get(m_pFormatCtx->metadata, aBuff, pTag, AV_DICT_IGNORE_SUFFIX);
        if (pTag) return {pTag->value};
        else return {};
    }
}

#ifdef OPT_CHAFA
void
Decoder::getAttachedPicture()
{
    int err = 0;
    AVStream* pStream {};

    for (int i = 0; i < (int)m_pFormatCtx->nb_streams; ++i)
    {
        auto* itStream = m_pFormatCtx->streams[i];
        if (itStream->disposition & AV_DISPOSITION_ATTACHED_PIC)
        {
            LogDebug("Found 'attached_pic'\n");
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
        LogDebug("avcodec_open2()\n");
        return;
    }

    LogDebug("codec name: '{}'\n", pCodec->long_name);

    m_pImgPacket = av_packet_alloc();
    err = av_read_frame(m_pFormatCtx, m_pImgPacket);
    if (err != 0) return;

    err = avcodec_send_packet(pCodecCtx, m_pImgPacket);
    if (err != 0) return;

    m_pImgFrame = av_frame_alloc();
    err = avcodec_receive_frame(pCodecCtx, m_pImgFrame);
    if (err == AVERROR(EINVAL))
    {
        LogDebug("err: {}, AVERROR(EINVAL)\n", err);
        return;
    }

    m_pSwsCtx = sws_getContext(
        m_pImgFrame->width, m_pImgFrame->height, (AVPixelFormat)m_pImgFrame->format,
        m_pImgFrame->width, m_pImgFrame->height, AV_PIX_FMT_RGB24,
        SWS_LANCZOS,
        {}, {}, {}
    );

    m_pConverted = av_frame_alloc();
    av_image_fill_linesizes(m_pConverted->linesize, AV_PIX_FMT_RGB24, m_pImgFrame->width);

    m_pConverted->format = AV_PIX_FMT_RGB24;
    m_pConverted->width = m_pImgFrame->width;
    m_pConverted->height = m_pImgFrame->height;

    LogDebug("format: {}, converted format: {}\n", m_pImgFrame->format, m_pConverted->format);

    err = av_frame_get_buffer(m_pConverted, 0);
    if (err != 0)
    {
        LogDebug("av_frame_get_buffer()\n");
        return;
    }

    sws_scale_frame(m_pSwsCtx, m_pConverted, m_pImgFrame);

    m_coverImg = Image {
        .pBuff = m_pConverted->data[0],
        .width = m_pConverted->width,
        .height = m_pConverted->height,
        .eFormat = covertFormat(m_pConverted->format)
    };
}
#else
    #define getAttachedPicture(...) (void)0
#endif

Image
Decoder::getCoverImage()
{
    return m_coverImg;
}

audio::ERROR
Decoder::open(StringView svPath)
{
    String sPathNullTerm = String(StdAllocator::inst(), svPath); /* with null char */
    defer( sPathNullTerm.destroy(StdAllocator::inst()) );

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
        LogDebug("avcodec_open2\n");
        return audio::ERROR::FAIL;
    }

    err = swr_alloc_set_opts2(&m_pSwr,
        &m_pStream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, m_pStream->codecpar->sample_rate,
        &m_pStream->codecpar->ch_layout, (AVSampleFormat)m_pStream->codecpar->format, m_pStream->codecpar->sample_rate,
        0, {}
    );

    LogDebug("codec name: '{}', ch_layout: {}, bit_rate: {}, sample_rate: {}\n",
        pCodec->long_name, m_pStream->codecpar->ch_layout.nb_channels, m_pStream->codecpar->bit_rate, m_pStream->codecpar->sample_rate
    );

    getAttachedPicture();

    return audio::ERROR::OK_;
}

audio::ERROR
Decoder::writeToRingBuffer(
    audio::RingBuffer* pRingBuff,
    [[maybe_unused]] const int nChannels,
    long* pSamplesWritten,
    adt::isize* pPcmPos
)
{
    if (!m_pStream) return audio::ERROR::END_OF_FILE;

    int err = 0;
    long nWrites = 0;

    *pSamplesWritten = 0;

    AVPacket packet {};
    while (av_read_frame(m_pFormatCtx, &packet) == 0)
    {
        defer(av_packet_unref(&packet));

        if (packet.stream_index != m_pStream->index)
            continue;
        err = avcodec_send_packet(m_pCodecCtx, &packet);
        if (err != 0 && err != AVERROR(EAGAIN))
            LogDebug("!EAGAIN\n");

        AVFrame frame {};
        while ((err = avcodec_receive_frame(m_pCodecCtx, &frame)) == 0)
        {
            defer(av_frame_unref(&frame));

            f64 currentTimeInSeconds = av_q2d(m_pStream->time_base) * (frame.best_effort_timestamp + frame.nb_samples);
            m_currentMS = currentTimeInSeconds * 1000.0;
            long pcmPos = currentTimeInSeconds * frame.ch_layout.nb_channels * frame.sample_rate;
            m_currentSamplePos = pcmPos;
            *pPcmPos = pcmPos;

            AVFrame res {};
            /* NOTE: not changing sample rate here, but on the mixer side instead. */
            res.sample_rate = frame.sample_rate;
            res.ch_layout = frame.ch_layout;
            res.format = AV_SAMPLE_FMT_FLT;
            defer(av_frame_unref(&res));

            swr_config_frame(m_pSwr, &res, &frame);
            err = swr_convert_frame(m_pSwr, &res, &frame);

            if (err < 0)
            {
                LogDebug("swr_convert_frame(): {}\n", av_err2str(err));
                continue;
            }

            const int nFrameSamples = res.nb_samples * res.ch_layout.nb_channels;

            const isize ringSize = pRingBuff->push(Span{reinterpret_cast<f32*>(res.data[0]), nFrameSamples});
            nWrites += nFrameSamples;

            /* NOTE: Fill until its good enough, but not all the way, since different audio drivers can ask for different amount of samples each update (like pipewire). */
            if (ringSize >= (pRingBuff->m_cap * 0.75))
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
    if (!m_pStream) return {};

    return m_pStream->codecpar->sample_rate;
}

void
Decoder::seekMS(f64 ms)
{
    if (!m_pCodecCtx) return;

	avcodec_flush_buffers(m_pCodecCtx);
	i64 pts = av_rescale_q(f64(ms) / 1000.0 * AV_TIME_BASE, AV_TIME_BASE_Q, m_pStream->time_base);
    av_seek_frame(m_pFormatCtx, m_audioStreamIdx, pts, 0);
}

i64
Decoder::getCurrentSamplePos()
{
    return m_currentSamplePos;
}

i64
Decoder::getTotalSamplesCount()
{
    if (!m_pFormatCtx || !m_pStream) return {};

    i64 res = (m_pFormatCtx->duration / (f64)AV_TIME_BASE) * m_pStream->codecpar->sample_rate * m_pStream->codecpar->ch_layout.nb_channels;
    return res;
}

int
Decoder::getChannelsCount()
{
    if (!m_pStream) return {};

    return m_pStream->codecpar->ch_layout.nb_channels;
}

} /* namespace platform::ffmpeg */

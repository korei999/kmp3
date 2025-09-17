#include "Decoder.hh"
#include "pfn.hh"

extern "C"
{

#include <libavutil/imgutils.h>

}

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
    if (m_pFormatCtx) pfn::avformat_close_input(&m_pFormatCtx);
    if (m_pCodecCtx) pfn::avcodec_free_context(&m_pCodecCtx);
    if (m_pSwr) pfn::swr_free(&m_pSwr);
    if (m_pImgPacket) pfn::av_packet_free(&m_pImgPacket);
    if (m_pImgFrame) pfn::av_frame_free(&m_pImgFrame);

    if (m_pTmpPacket) pfn::av_packet_free(&m_pTmpPacket);
    if (m_pTmpFrame) pfn::av_frame_free(&m_pTmpFrame);
    if (m_pCvtFrame) pfn::av_frame_free(&m_pCvtFrame);

#ifdef OPT_CHAFA
    if (m_pConverted) pfn::av_frame_free(&m_pConverted);
    if (m_pSwsCtx) pfn::sws_freeContext(m_pSwsCtx), m_pSwsCtx = {};
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
    if (!pfn::loadSO()) throw RuntimeException("failure while trying to load ffmpeg libraries...");
    new(&m_mtx) Mutex {Mutex::TYPE::PLAIN};
    return *this;
}

void
Decoder::destroy()
{
    {
        LockScope lockGuard {&m_mtx};
        close();
    }
    m_mtx.destroy();
    pfn::unloadSO();

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
    pTag = pfn::av_dict_get(m_pStream->metadata, aBuff, pTag, AV_DICT_IGNORE_SUFFIX);

    if (pTag)
    {
        return {pTag->value};
    }
    else
    {
        pTag = pfn::av_dict_get(m_pFormatCtx->metadata, aBuff, pTag, AV_DICT_IGNORE_SUFFIX);
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

    const AVCodec* pCodec = pfn::avcodec_find_decoder(pStream->codecpar->codec_id);
    if (!pCodec) return;

    auto* pCodecCtx = pfn::avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) return;
    defer( pfn::avcodec_free_context(&pCodecCtx) );

    pfn::avcodec_parameters_to_context(pCodecCtx, pStream->codecpar);
    err = pfn::avcodec_open2(pCodecCtx, pCodec, {});
    if (err < 0)
    {
        LogDebug("avcodec_open2()\n");
        return;
    }

    LogDebug("codec name: '{}'\n", pCodec->long_name);

    m_pImgPacket = pfn::av_packet_alloc();
    err = pfn::av_read_frame(m_pFormatCtx, m_pImgPacket);
    if (err != 0) return;

    err = pfn::avcodec_send_packet(pCodecCtx, m_pImgPacket);
    if (err != 0) return;

    m_pImgFrame = pfn::av_frame_alloc();
    err = pfn::avcodec_receive_frame(pCodecCtx, m_pImgFrame);
    if (err == AVERROR(EINVAL))
    {
        LogDebug("err: {}, AVERROR(EINVAL)\n", err);
        return;
    }

    m_pSwsCtx = pfn::sws_getContext(
        m_pImgFrame->width, m_pImgFrame->height, (AVPixelFormat)m_pImgFrame->format,
        m_pImgFrame->width, m_pImgFrame->height, AV_PIX_FMT_RGB24,
        SWS_LANCZOS,
        {}, {}, {}
    );

    m_pConverted = pfn::av_frame_alloc();
    pfn::av_image_fill_linesizes(m_pConverted->linesize, AV_PIX_FMT_RGB24, m_pImgFrame->width);

    m_pConverted->format = AV_PIX_FMT_RGB24;
    m_pConverted->width = m_pImgFrame->width;
    m_pConverted->height = m_pImgFrame->height;

    LogDebug("format: {}, converted format: {}\n", m_pImgFrame->format, m_pConverted->format);

    err = pfn::av_frame_get_buffer(m_pConverted, 0);
    if (err != 0)
    {
        LogDebug("av_frame_get_buffer()\n");
        return;
    }

    pfn::sws_scale_frame(m_pSwsCtx, m_pConverted, m_pImgFrame);

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
    String sPathNullTerm = String(Gpa::inst(), svPath); /* with null char */
    defer( sPathNullTerm.destroy(Gpa::inst()) );

    pfn::av_log_set_level(AV_LOG_QUIET);

    int err = 0;
    defer( if (err < 0) close() );

    err = pfn::avformat_open_input(&m_pFormatCtx, sPathNullTerm.data(), {}, {});
    if (err != 0) return audio::ERROR::FAIL;

    err = pfn::avformat_find_stream_info(m_pFormatCtx, {});
    if (err != 0) return audio::ERROR::FAIL;

    int idx = pfn::av_find_best_stream(m_pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, {}, 0);
    if (idx < 0) return audio::ERROR::FAIL;

    m_audioStreamIdx = idx;
    m_pStream = m_pFormatCtx->streams[idx];

    const AVCodec* pCodec = pfn::avcodec_find_decoder(m_pStream->codecpar->codec_id);
    if (!pCodec) return audio::ERROR::FAIL;

    m_pCodecCtx = pfn::avcodec_alloc_context3(pCodec);
    if (!m_pCodecCtx) return audio::ERROR::FAIL;

    pfn::avcodec_parameters_to_context(m_pCodecCtx, m_pStream->codecpar);

    err = pfn::avcodec_open2(m_pCodecCtx, pCodec, {});
    if (err < 0)
    {
        LogDebug("avcodec_open2\n");
        return audio::ERROR::FAIL;
    }

    err = pfn::swr_alloc_set_opts2(&m_pSwr,
        &m_pStream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, m_pStream->codecpar->sample_rate,
        &m_pStream->codecpar->ch_layout, (AVSampleFormat)m_pStream->codecpar->format, m_pStream->codecpar->sample_rate,
        0, {}
    );

    LogDebug("codec name: '{}', ch_layout: {}, bit_rate: {}, sample_rate: {}\n",
        pCodec->long_name, m_pStream->codecpar->ch_layout.nb_channels, m_pStream->codecpar->bit_rate, m_pStream->codecpar->sample_rate
    );

    getAttachedPicture();

    m_pTmpPacket = pfn::av_packet_alloc();
    m_pTmpFrame = pfn::av_frame_alloc();
    m_pCvtFrame = pfn::av_frame_alloc();

    return audio::ERROR::OK_;
}

audio::ERROR
Decoder::writeToRingBuffer(
    audio::RingBuffer* pRingBuff,
    [[maybe_unused]] const int nChannels,
    [[maybe_unused]] const audio::PCM_TYPE ePcmType,
    long* pSamplesWritten,
    isize* pPcmPos
)
{
    if (!m_pStream) return audio::ERROR::END_OF_FILE;

    int err = 0;
    long nWrites = 0;

    *pSamplesWritten = 0;

    AVPacket* pPacket = m_pTmpPacket;
    while (pfn::av_read_frame(m_pFormatCtx, pPacket) == 0)
    {
        defer( pfn::av_packet_unref(pPacket) );

        if (pPacket->stream_index != m_pStream->index) continue;

        err = pfn::avcodec_send_packet(m_pCodecCtx, pPacket);
        if (err != 0 && err != AVERROR(EAGAIN))
            LogWarn("!EAGAIN\n");

        AVFrame* pFrame = m_pTmpFrame;
        while ((err = pfn::avcodec_receive_frame(m_pCodecCtx, pFrame)) == 0)
        {
            defer( pfn::av_frame_unref(pFrame) );

            f64 currentTimeInSeconds = av_q2d(m_pStream->time_base) * (pFrame->best_effort_timestamp + pFrame->nb_samples);
            m_currentMS = currentTimeInSeconds * 1000.0;
            long pcmPos = currentTimeInSeconds * pFrame->ch_layout.nb_channels * pFrame->sample_rate;
            m_currentSamplePos = pcmPos;
            *pPcmPos = pcmPos;

            /* NOTE: not changing sample rate here, but on the mixer side instead. */
            AVFrame* pRes = m_pCvtFrame;
            pRes->sample_rate = pFrame->sample_rate;
            pRes->ch_layout = pFrame->ch_layout;
            pRes->format = AV_SAMPLE_FMT_FLT;

            pfn::swr_config_frame(m_pSwr, pRes, pFrame);
            err = pfn::swr_convert_frame(m_pSwr, pRes, pFrame);
            defer( pfn::av_frame_unref(pRes) );

            if (err < 0)
            {
                char aBuff[AV_ERROR_MAX_STRING_SIZE] {};
                const int n = pfn::av_strerror(err, aBuff, sizeof(aBuff));
                LogError("swr_convert_frame(): {}\n", Span{aBuff, n});
                continue;
            }

            const int nFrameSamples = pRes->nb_samples * pRes->ch_layout.nb_channels;
            const isize ringSize = pRingBuff->push(Span{reinterpret_cast<f32*>(pRes->data[0]), nFrameSamples});
            nWrites += nFrameSamples;

            if (ringSize >= audio::RING_BUFFER_HIGH_THRESHOLD)
            {
                *pSamplesWritten = nWrites;
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

    pfn::avcodec_flush_buffers(m_pCodecCtx);
    i64 pts = pfn::av_rescale_q(f64(ms) / 1000.0 * AV_TIME_BASE, AV_TIME_BASE_Q, m_pStream->time_base);
    pfn::av_seek_frame(m_pFormatCtx, m_audioStreamIdx, pts, 0);
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

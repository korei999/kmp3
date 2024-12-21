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
Reader::close()
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
    super = {&inl_ReaderVTable};
}

Opt<String>
Reader::getMetadataValue(const String sKey)
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
static void
getAttachedPicture(Reader* s)
{
    int err = 0;
    AVStream* pStream {};

    for (u32 i = 0; i < s->m_pFormatCtx->nb_streams; ++i)
    {
        auto* itStream = s->m_pFormatCtx->streams[i];
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

    s->m_pImgPacket = av_packet_alloc();
    err = av_read_frame(s->m_pFormatCtx, s->m_pImgPacket);
    if (err != 0) return;

    err = avcodec_send_packet(pCodecCtx, s->m_pImgPacket);
    if (err != 0) return;

    s->m_pImgFrame = av_frame_alloc();
    err = avcodec_receive_frame(pCodecCtx, s->m_pImgFrame);
    if (err == AVERROR(EINVAL))
        LOG_BAD("err: {}, AVERROR(EINVAL)\n", err);

    s->m_pSwsCtx = sws_getContext(
        s->m_pImgFrame->width, s->m_pImgFrame->height, (AVPixelFormat)s->m_pImgFrame->format,
        s->m_pImgFrame->width, s->m_pImgFrame->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, {}, {}, {}
    );

    s->m_pConverted = av_frame_alloc();
    s->m_pConverted->format = AV_PIX_FMT_RGB24;
    s->m_pConverted->width = s->m_pImgFrame->width;
    s->m_pConverted->height = s->m_pImgFrame->height;

    err = av_frame_get_buffer(s->m_pConverted, 0);
    if (err != 0) { LOG_BAD("av_frame_get_buffer()\n"); return; }

    sws_scale(s->m_pSwsCtx,
        s->m_pImgFrame->data, s->m_pImgFrame->linesize, 0, s->m_pImgFrame->height,
        s->m_pConverted->data, s->m_pConverted->linesize
    );

    LOG_WARN("width: {}, height: {}, format: {}\n", s->m_pImgFrame->width, s->m_pImgFrame->height, s->m_pImgFrame->format);

    s->m_oCoverImg = Image {
        .pBuff = s->m_pConverted->data[0],
        .width = u32(s->m_pConverted->width),
        .height = u32(s->m_pConverted->height),
        .eFormat = covertFormat(s->m_pConverted->format)
    };
}
#else
    #define getAttachedPicture(...) (void)0
#endif

Opt<Image>
Reader::getCoverImage()
{
    return m_oCoverImg;
}

audio::ERROR
Reader::open(String sPath)
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

    LOG_NOTIFY("codec name: '{}'\n", pCodec->long_name);

    getAttachedPicture(this);

    return audio::ERROR::OK_;
}

audio::ERROR
Reader::writeToBuffer(
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
Reader::getSampleRate()
{
    return m_pStream->codecpar->sample_rate;
}

static void
seekFrame(Reader* s, u64 frame)
{
    avcodec_flush_buffers(s->m_pCodecCtx);
    avformat_seek_file(s->m_pFormatCtx, s->m_pStream->index, 0, frame, frame, AVSEEK_FLAG_BACKWARD);
}

void
Reader::seekMS(long ms)
{
    auto frameNumber = av_rescale(ms, m_pStream->time_base.den, m_pStream->time_base.num);
    frameNumber /= 1000;
    seekFrame(this, frameNumber);
}

long
Reader::getCurrentSamplePos()
{
    return m_currentSamplePos;
}

long
Reader::getTotalSamplesCount()
{
    long res = (m_pFormatCtx->duration / (f32)AV_TIME_BASE) * m_pStream->codecpar->sample_rate * m_pStream->codecpar->ch_layout.nb_channels;
    return res;
}

int
Reader::getChannelsCount()
{
    return m_pStream->codecpar->ch_layout.nb_channels;
}

} /* namespace ffmpeg */

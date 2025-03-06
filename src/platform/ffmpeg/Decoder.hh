#pragma once

#include "adt/String.hh"
#include "audio.hh"
#include "Image.hh"

extern "C"
{

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#ifdef OPT_CHAFA
#include <libswscale/swscale.h>
#endif

}

using namespace adt;

namespace platform::ffmpeg
{

struct Decoder : audio::IDecoder
{
    [[nodiscard]] virtual audio::ERROR writeToBuffer(
        Span<f32> spBuff, const int nFrames, const int nChannles,
        long* pSamplesWritten, ssize* pPcmPos
    ) override final;

    [[nodiscard]] virtual u32 getSampleRate() override final;
    virtual void seekMS(f64 ms) override final;
    [[nodiscard]] virtual i64 getCurrentSamplePos() override final;
    [[nodiscard]] virtual i64 getCurrentMS() override final;
    [[nodiscard]] virtual i64 getTotalMS() override final;
    [[nodiscard]] virtual i64 getTotalSamplesCount() override final;
    [[nodiscard]] virtual int getChannelsCount() override final;
    [[nodiscard]] virtual StringView getMetadataValue(const StringView svKey) override final;
    [[nodiscard]] virtual Image getCoverImage() override final;
    [[nodiscard]] virtual audio::ERROR open(StringView sPath) override final;
    virtual void close() override final;

    /* */

private:
    AVStream* m_pStream {};
    AVFormatContext* m_pFormatCtx {};
    AVCodecContext* m_pCodecCtx {};
    SwrContext* m_pSwr {};
    int m_audioStreamIdx {};
    u64 m_currentSamplePos {};
    f64 m_currentMS {};

    AVPacket* m_pImgPacket {};
    AVFrame* m_pImgFrame {};
    Image m_coverImg {};

#ifdef OPT_CHAFA
    SwsContext* m_pSwsCtx {};
    AVFrame* m_pConverted {};
#endif

    void getAttachedPicture();
};

} /* namespace platform::ffmpeg */

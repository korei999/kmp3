#pragma once

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

namespace platform::ffmpeg
{

struct Decoder : audio::IDecoder
{
    [[nodiscard]] virtual audio::ERROR writeToRingBuffer(
        audio::RingBuffer* pRingBuff,
        const int nChannels,
        const audio::PCM_TYPE ePcmType,
        long* pSamplesWritten,
        adt::isize* pPcmPos
    ) override final;

    virtual Decoder& init() override final;
    virtual void destroy() override final;
    [[nodiscard]] virtual adt::u32 getSampleRate() override final;
    virtual void seekMS(adt::f64 ms) override final;
    [[nodiscard]] virtual adt::i64 getCurrentSamplePos() override final;
    [[nodiscard]] virtual adt::i64 getCurrentMS() override final;
    [[nodiscard]] virtual adt::i64 getTotalMS() override final;
    [[nodiscard]] virtual adt::i64 getTotalSamplesCount() override final;
    [[nodiscard]] virtual int getChannelsCount() override final;
    [[nodiscard]] virtual adt::StringView getMetadata(const adt::StringView svKey) override final;
    [[nodiscard]] virtual Image getCoverImage() override final;
    [[nodiscard]] virtual audio::ERROR open(adt::StringView sPath) override final;
    virtual void close() override final;

    /* */

    AVStream* m_pStream {};
    AVFormatContext* m_pFormatCtx {};
    AVCodecContext* m_pCodecCtx {};
    SwrContext* m_pSwr {};
    int m_audioStreamIdx {};
    adt::u64 m_currentSamplePos {};
    adt::f64 m_currentMS {};

    AVPacket* m_pImgPacket {};
    AVFrame* m_pImgFrame {};
    Image m_coverImg {};

#ifdef OPT_CHAFA
    SwsContext* m_pSwsCtx {};
    AVFrame* m_pConverted {};
#endif


    AVPacket* m_pTmpPacket {};
    AVFrame* m_pTmpFrame {};
    AVFrame* m_pCvtFrame {};

    /* */

    void getAttachedPicture();
};

} /* namespace platform::ffmpeg */

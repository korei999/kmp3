#pragma once

#include "adt/Opt.hh"
#include "adt/String.hh"
#include "audio.hh"
#include "Image.hh"

extern "C"
{

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#ifdef USE_CHAFA
#include <libswscale/swscale.h>
#endif

}

using namespace adt;

namespace ffmpeg
{

struct Decoder : audio::IDecoder
{
    [[nodiscard]] virtual audio::ERROR writeToBuffer(
        f32* pBuff, const int buffSize, const int nFrames, const int nChannles,
        long* pSamplesWritten, s64* pPcmPos
    ) override final;

    [[nodiscard]] virtual u32 getSampleRate() override final;
    virtual void seekMS(f64 ms) override final;
    [[nodiscard]] virtual s64 getCurrentSamplePos() override final;
    [[nodiscard]] virtual s64 getCurrentMS() override final;
    [[nodiscard]] virtual s64 getTotalMS() override final;
    [[nodiscard]] virtual s64 getTotalSamplesCount() override final;
    [[nodiscard]] virtual int getChannelsCount() override final;
    [[nodiscard]] virtual Opt<String> getMetadataValue(const String sKey) override final;
    [[nodiscard]] virtual Opt<Image> getCoverImage() override final;
    [[nodiscard]] virtual audio::ERROR open(String sPath) override final;
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
    Opt<Image> m_oCoverImg {};

#ifdef USE_CHAFA
    SwsContext* m_pSwsCtx {};
    AVFrame* m_pConverted {};
#endif

    void getAttachedPicture();
};

} /* namespace ffmpeg */

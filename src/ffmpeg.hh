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

struct Reader : audio::IReader
{
    [[nodiscard]] virtual audio::ERROR writeToBuffer(
        f32* pBuff, const u32 buffSize, const u32 nFrames, const u32 nChannles,
        long* pSamplesWritten, u64* pPcmPos
    ) override final;

    [[nodiscard]] virtual u32 getSampleRate() override final;
    virtual void seekMS(long ms) override final;
    [[nodiscard]] virtual long getCurrentSamplePos() override final;
    [[nodiscard]] virtual long getTotalSamplesCount() override final;
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

    AVPacket* m_pImgPacket {};
    AVFrame* m_pImgFrame {};
    Opt<Image> m_oCoverImg {};

#ifdef USE_CHAFA
    SwsContext* m_pSwsCtx {};
    AVFrame* m_pConverted {};
#endif

    void getAttachedPicture();
    void seekFrame(u64 frame);
};

} /* namespace ffmpeg */

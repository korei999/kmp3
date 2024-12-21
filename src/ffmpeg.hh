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

struct Reader
{
    audio::IReader super {};

    /* */

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

    /* */

    Reader() = default;
    Reader(INIT_FLAG eFlag);

    /* */

    [[nodiscard]] audio::ERROR writeToBuffer(
        f32* pBuff, const u32 buffSize, const u32 nFrames, const u32 nChannles,
        long* pSamplesWritten, u64* pPcmPos
    );

    [[nodiscard]] u32 getSampleRate();
    void seekMS(long ms);
    [[nodiscard]] long getCurrentSamplePos();
    [[nodiscard]] long getTotalSamplesCount();
    [[nodiscard]] int getChannelsCount();
    [[nodiscard]] Opt<String> getMetadataValue(const String sKey);
    [[nodiscard]] Opt<Image> getCoverImage();
    [[nodiscard]] audio::ERROR open(String sPath);
    void close();
};

inline const audio::ReaderVTable inl_ReaderVTable = audio::ReaderVTableGenerate<Reader>();

inline
Reader::Reader(INIT_FLAG eFlag)
{
    if (eFlag == INIT_FLAG::INIT) super = {&inl_ReaderVTable};
}

} /* namespace ffmpeg */

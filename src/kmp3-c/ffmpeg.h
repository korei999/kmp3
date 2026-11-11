#pragma once

#include "audio.h"
#include "Image.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#ifdef OPT_CHAFA
    #include <libswscale/swscale.h>
#endif

typedef enum
{
    FFMPEG_ERROR_OK = 0,
    FFMPEG_ERROR_END_OF_FILE,
    FFMPEG_ERROR_DONE,
    FFMPEG_ERROR_FAIL,
} FFMPEG_ERROR;

typedef struct ffmpeg_Decoder ffmpeg_Decoder;
struct ffmpeg_Decoder
{
    k_Mutex mtx;

    AVStream* pStream;
    AVFormatContext* pFormatCtx;
    AVCodecContext* pCodecCtx;
    SwrContext* pSwr;
    int audioStreamIdx;
    uint64_t currentSamplePos;
    double currentMS;

    AVPacket* pImgPacket;
    AVFrame* pImgFrame;

    Image coverImg;

#ifdef OPT_CHAFA
    SwsContext* pSwsCtx;
    AVFrame* pConverted;
#endif

    AVPacket* pTmpPacket;
    AVFrame* pTmpFrame;
    AVFrame* pCvtFrame;
};

K_NO_DISCARD bool ffmpeg_DecoderInit(ffmpeg_Decoder* s);
void ffmpeg_DecoderDestroy(ffmpeg_Decoder* s);
K_NO_DISCARD FFMPEG_ERROR ffmpeg_DecoderWriteToRing(ffmpeg_Decoder* s, audio_RingBuffer* pRing, int nChannels, ssize_t* pNSamplesWritten, ssize_t* pPcmPos);
K_NO_DISCARD uint32_t ffmpeg_DecoderGetSampleRate(ffmpeg_Decoder* s);
void ffmpeg_DecoderSeekMS(ffmpeg_Decoder* s, double ms);
K_NO_DISCARD int64_t ffmpeg_DecoderGetCurrentSamplePos(ffmpeg_Decoder* s);
K_NO_DISCARD int64_t ffmpeg_DecoderGetCurrentMS(ffmpeg_Decoder* s);
K_NO_DISCARD int64_t ffmpeg_DecoderGetTotalMS(ffmpeg_Decoder* s);
K_NO_DISCARD int64_t ffmpeg_DecoderGetTotalSamplesCount(ffmpeg_Decoder* s);
K_NO_DISCARD int ffmpeg_DecoderGetChannelsCount(ffmpeg_Decoder* s);
K_NO_DISCARD k_StringView ffmpeg_DecoderGetMetadata(ffmpeg_Decoder* s, const k_StringView svKey);
K_NO_DISCARD FFMPEG_ERROR ffmpeg_DecoderOpen(ffmpeg_Decoder* s, k_StringView svPath);
void ffmpeg_DecoderClose(ffmpeg_Decoder* s);
K_NO_DISCARD Image ffmpeg_DecoderGetCoverImage(ffmpeg_Decoder* s);

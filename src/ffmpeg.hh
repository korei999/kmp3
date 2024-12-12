#pragma once

#include "adt/Opt.hh"
#include "adt/String.hh"

using namespace adt;

namespace ffmpeg
{

enum class ERROR : u8
{
    OK_ = 0,
    UNKNOWN,
    END_OF_FILE,
    DONE,
    FILE_OPENING,
    AUDIO_STREAM_NOT_FOUND,
    DECODER_NOT_FOUND,
    DECODING_CONTEXT_ALLOCATION,
    CODEC_CONTEXT_PARAMETERS,
    INITIALIZING_DECODER,
    FRAME_ALLOC,
    CODEC_OPEN,
};

constexpr String mapERRORToString[] {
    "OK_",
    "UNKNOWN",
    "EOF_OF_FILE",
    "DONE",
    "FILE_OPENING",
    "AUDIO_STREAM_NOT_FOUND",
    "DECODER_NOT_FOUND",
    "DECODING_CONTEXT_ALLOCATION",
    "CODEC_CONTEXT_PARAMETERS",
    "INITIALIZING_DECODER",
    "FRAME_ALLOC",
    "CODEC_OPEN",
};

enum class PIXEL_FORMAT : int
{
    NONE = -1,
    RGBA8_PREMULTIPLIED,
    RGBA8_UNASSOCIATED,
    RGB8,
};

struct Image
{
    u8* pBuff {};
    u32 width {};
    u32 height {};
    PIXEL_FORMAT eFormat {};
};

struct Decoder;

[[nodiscard]] Decoder* DecoderAlloc(IAllocator* pAlloc);
void DecoderClose(Decoder* s);
[[nodiscard]] Opt<String> DecoderGetMetadataValue(Decoder* s, const String sKey);
[[nodiscard]] Opt<ffmpeg::Image> DecoderGetCoverImage(Decoder* s);
[[nodiscard]] ERROR DecoderOpen(Decoder* s, String sPath);

[[nodiscard]] ERROR
DecoderWriteToBuffer(
    Decoder* s,
    f32* pBuff,
    const u32 buffSize,
    const u32 nFrames,
    const u32 nChannles,
    long* pSamplesWritten,
    u64* pPcmPos
);

[[nodiscard]] u32 DecoderGetSampleRate(Decoder* s);
void DecoderSeekMS(Decoder* s, long ms);
[[nodiscard]] long DecoderGetCurrentSamplePos(Decoder* s);
[[nodiscard]] long DecoderGetTotalSamplesCount(Decoder* s);
[[nodiscard]] int DecoderGetChannelsCount(Decoder* s);

} /* namespace ffmpeg */

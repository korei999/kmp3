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
    "OK",
    "UNKNOWN",
    "EOF_",
    "DONE",
    "FILE_OPENING",
    "AUDIO_STREAM_NOT_FOUND",
    "DECODER_NOT_FOUND",
    "DECODING_CONTEXT_ALLOCATION",
    "CODEC_CONTEXT_PARAMETERS",
    "INITIALIZING_DECODER",
    "AV_FRAME_ALLOC"
};

// SIXEL_PIXELFORMAT_RGB555   (SIXEL_FORMATTYPE_COLOR     | 0x01) /* 15bpp */
// SIXEL_PIXELFORMAT_RGB565   (SIXEL_FORMATTYPE_COLOR     | 0x02) /* 16bpp */
// SIXEL_PIXELFORMAT_RGB888   (SIXEL_FORMATTYPE_COLOR     | 0x03) /* 24bpp */
// SIXEL_PIXELFORMAT_BGR555   (SIXEL_FORMATTYPE_COLOR     | 0x04) /* 15bpp */
// SIXEL_PIXELFORMAT_BGR565   (SIXEL_FORMATTYPE_COLOR     | 0x05) /* 16bpp */
// SIXEL_PIXELFORMAT_BGR888   (SIXEL_FORMATTYPE_COLOR     | 0x06) /* 24bpp */
// SIXEL_PIXELFORMAT_ARGB8888 (SIXEL_FORMATTYPE_COLOR     | 0x10) /* 32bpp */
// SIXEL_PIXELFORMAT_RGBA8888 (SIXEL_FORMATTYPE_COLOR     | 0x11) /* 32bpp */
// SIXEL_PIXELFORMAT_ABGR8888 (SIXEL_FORMATTYPE_COLOR     | 0x12) /* 32bpp */
// SIXEL_PIXELFORMAT_BGRA8888 (SIXEL_FORMATTYPE_COLOR     | 0x13) /* 32bpp */
// SIXEL_PIXELFORMAT_G1       (SIXEL_FORMATTYPE_GRAYSCALE | 0x00) /* 1bpp grayscale */
// SIXEL_PIXELFORMAT_G2       (SIXEL_FORMATTYPE_GRAYSCALE | 0x01) /* 2bpp grayscale */
// SIXEL_PIXELFORMAT_G4       (SIXEL_FORMATTYPE_GRAYSCALE | 0x02) /* 4bpp grayscale */
// SIXEL_PIXELFORMAT_G8       (SIXEL_FORMATTYPE_GRAYSCALE | 0x03) /* 8bpp grayscale */
// SIXEL_PIXELFORMAT_AG88     (SIXEL_FORMATTYPE_GRAYSCALE | 0x13) /* 16bpp gray+alpha */
// SIXEL_PIXELFORMAT_GA88     (SIXEL_FORMATTYPE_GRAYSCALE | 0x23) /* 16bpp gray+alpha */
// SIXEL_PIXELFORMAT_PAL1     (SIXEL_FORMATTYPE_PALETTE   | 0x00) /* 1bpp palette */
// SIXEL_PIXELFORMAT_PAL2     (SIXEL_FORMATTYPE_PALETTE   | 0x01) /* 2bpp palette */
// SIXEL_PIXELFORMAT_PAL4     (SIXEL_FORMATTYPE_PALETTE   | 0x02) /* 4bpp palette */
// SIXEL_PIXELFORMAT_PAL8     (SIXEL_FORMATTYPE_PALETTE   | 0x03) /* 8bpp palette */

enum class PIXEL_FORMAT : int
{
    NONE,
    RGB555,
    RGB565,
    RGB888,
    // BGR555,
    // BGR565,
    // BGR888,
    // ARGB8888,
    RGBA8888,
    // ABGR8888,
    // BGRA8888,
    // G1,
    // G2,
    // G4,
    // G8,
    // AG88,
    // GA88,
    // PAL1,
    // PAL2,
    // PAL4,
    // PAL8,
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
Opt<Image> DecoderGetPicture(Decoder* s);
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

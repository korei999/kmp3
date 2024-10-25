#pragma once

#include "adt/String.hh"

using namespace adt;

namespace ffmpeg
{

enum ERROR : u8
{
    OK = 0,
    UNKNOWN,
    EOF_,
    DONE,
    FILE_OPENING,
    AUDIO_STREAM_NOT_FOUND,
    DECODER_NOT_FOUND,
    DECODING_CONTEXT_ALLOCATION,
    CODEC_CONTEXT_PARAMETERS,
    INITIALIZING_DECODER,
    AV_FRAME_ALLOC,
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

struct Decoder;

[[nodiscard]] Decoder* DecoderAlloc(Allocator* pAlloc);
void DecoderClose(Decoder* s);
[[nodiscard]] ERROR DecoderOpen(Decoder* s, String sPath);
[[nodiscard]] ERROR DecoderWriteToBuffer(Decoder* s, f32* pBuff, u32 buffSize, u32 nFrames, long* pSamplesWritten);

} /* namespace ffmpeg */

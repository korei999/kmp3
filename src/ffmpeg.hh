#pragma once

#include "adt/Opt.hh"
#include "adt/String.hh"
#include "audio.hh"
#include "Image.hh"

using namespace adt;

namespace ffmpeg
{

struct Decoder;

[[nodiscard]] Decoder* DecoderAlloc(IAllocator* pAlloc);
void DecoderClose(Decoder* s);
[[nodiscard]] Opt<String> DecoderGetMetadataValue(Decoder* s, const String sKey);
[[nodiscard]] Opt<Image> DecoderGetCoverImage(Decoder* s);
[[nodiscard]] audio::ERROR DecoderOpen(Decoder* s, String sPath);

[[nodiscard]] audio::ERROR
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

struct Reader
{
    audio::IReader super {};

    /* */

    Decoder* m_pDecoder {}; /* poor man's pimpl */

    /* */

    Reader() = default;
    Reader(IAllocator* pAlloc);

    /* */

    [[nodiscard]] audio::ERROR writeToBuffer(
        f32* pBuff, const u32 buffSize, const u32 nFrames, const u32 nChannles,
        long* pSamplesWritten, u64* pPcmPos
    ) { return DecoderWriteToBuffer(m_pDecoder, pBuff, buffSize, nFrames, nChannles, pSamplesWritten, pPcmPos); }

    [[nodiscard]] u32 getSampleRate() { return DecoderGetSampleRate(m_pDecoder); }
    void seekMS(long ms) { return DecoderSeekMS(m_pDecoder, ms); }
    [[nodiscard]] long getCurrentSamplePos() { return DecoderGetCurrentSamplePos(m_pDecoder); }
    [[nodiscard]] long getTotalSamplesCount() { return DecoderGetTotalSamplesCount(m_pDecoder); }
    [[nodiscard]] int getChannelsCount() { return DecoderGetChannelsCount(m_pDecoder); }
    [[nodiscard]] Opt<String> getMetadataValue(const String sKey) { return DecoderGetMetadataValue(m_pDecoder, sKey); }
    [[nodiscard]] Opt<Image> getCoverImage() { return DecoderGetCoverImage(m_pDecoder); }
    [[nodiscard]] audio::ERROR open(String sPath) { return DecoderOpen(m_pDecoder, sPath); }
    void close() { DecoderClose(m_pDecoder); }
};

inline const audio::ReaderVTable inl_ReaderVTable = audio::ReaderVTableGenerate<Reader>();

} /* namespace ffmpeg */

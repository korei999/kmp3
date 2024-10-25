#include "ffmpeg.hh"

#include "adt/Arena.hh"
#include "adt/defer.hh"
#include "adt/types.hh"
#include "adt/utils.hh"
#include "adt/OsAllocator.hh"
#include "logs.hh"

extern "C"
{

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>

}

/* https://gist.github.com/jcelerier/c28310e34c189d37e99609be8cd74bdcG*/
#define RAW_OUT_ON_PLANAR true

namespace ffmpeg
{

struct Decoder
{
    String path {};
    AVStream* pStream {};
    AVFormatContext* pFormatCtx {};
    AVCodecContext* pCodecCtx {};
    AVFrame* pFrame {};
    AVPacket* pPacket {};
    AVAudioFifo* pFifo {};
    int audioStreamIdx {};
    u32 buffPos {};
};

struct Buffer
{
    f32* pBuff {};
    u32 buffSize {};
    u32 idx {};
};

static inline bool
BufferPush(Buffer* s, f32 x)
{
    if (s->idx < s->buffSize)
    {
        /*LOG_WARN("PUSH: {}\n", x);*/
        s->pBuff[s->idx++] = x;
        return true;
    }

    return false;
}

static inline bool
BufferCopy(Buffer* s, void* pData, u32 memSize, u32 nMembers)
{
    /*LOG_WARN("COPY\n");*/
    if ((s->buffSize - s->idx) * sizeof(*s->pBuff) < memSize*nMembers)
    {
        s->idx += (memSize*nMembers) / sizeof(*s->pBuff);
        memcpy(s->pBuff, pData, memSize*nMembers);
        return true;
    }

    return false;
}

Decoder*
DecoderAlloc(Allocator* pAlloc)
{
    Decoder* s = (Decoder*)alloc(pAlloc, 1, sizeof(Decoder));
    if (!s) return nullptr;

    *s = {};
    return s;
}

void
DecoderClose(Decoder* s)
{
    /* TODO: cleanup... */

    if (s->pFormatCtx) avformat_close_input(&s->pFormatCtx);
    if (s->pCodecCtx) avcodec_free_context(&s->pCodecCtx);
    if (s->pFrame) av_frame_free(&s->pFrame);
    if (s->pPacket) av_packet_free(&s->pPacket);

    *s = {};
}

/*static FILE* s_outFile;*/

int
printError(const char* pPrefix, int errorCode)
{
    if (errorCode == 0)
    {
        return 0;
    }
    else
    {
        char buf[64] {};

        if (av_strerror(errorCode, buf, utils::size(buf)) != 0)
            strcpy(buf, "UNKNOWN_ERROR");

        CERR("{} ({}: {})\n", pPrefix, errorCode, buf);

        return errorCode;
    }
}

/**
 * Extract a single sample and convert to f32.
 */
f32
getSample(const AVCodecContext* pCodecCtx, u8* pBuff, int sampleIndex)
{
    s64 val = 0;
    f32 ret = 0;
    int sampleSize = av_get_bytes_per_sample(pCodecCtx->sample_fmt);
    switch (sampleSize)
    {
        case 1:
            /* 8bit samples are always unsigned */
            val = ((u8*)pBuff)[sampleIndex];
            /* make signed */
            val -= 127;
            break;

        case 2:
            val = ((s16*)pBuff)[sampleIndex];
            break;

        case 4:
            val = ((s32*)pBuff)[sampleIndex];
            break;

        case 8:
            val = ((s64*)pBuff)[sampleIndex];
            break;

        default:
            LOG_WARN("Invalid sample size {}.\n", sampleSize);
            return 0;
    }

    // Check which data type is in the sample.
    switch (pCodecCtx->sample_fmt)
    {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_U8P:
        case AV_SAMPLE_FMT_S16P:
        case AV_SAMPLE_FMT_S32P:
            /* integer => Scale to [-1, 1] and convert to f32. */
            ret = f32(val) / f32((1 << (sampleSize*8 - 1)) - 1);
            break;

        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            /* f32 => reinterpret */
            ret = *(f32*)&val;
            break;

        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            /* double => reinterpret and then static cast down */
            ret = *(f64*)&val;
            break;

        default:
            LOG_WARN("Invalid sample format {}.\n", av_get_sample_fmt_name(pCodecCtx->sample_fmt));
            return 0;
    }

    return ret;
}

/**
 * Write the frame to an output file.
 */
bool
handleFrame(const AVCodecContext* pCodecCtx, const AVFrame* pFrame, Buffer* pBW)
{
    if (av_sample_fmt_is_planar(pCodecCtx->sample_fmt) == 1)
    {
        // This means that the data of each channel is in its own buffer.
        // => frame->extended_data[i] contains data for the i-th channel.
        for (int s = 0; s < pFrame->nb_samples; ++s)
        {
            for (int c = 0; c < pCodecCtx->ch_layout.nb_channels; ++c)
            {
                f32 sample = getSample(pCodecCtx, pFrame->extended_data[c], s);
                if (!BufferPush(pBW, sample)) return false;
            }
        }
    }
    else
    {
        // This means that the data of each channel is in the same buffer.
        // => frame->extended_data[0] contains data of all channels.
        if (RAW_OUT_ON_PLANAR)
        {
            /*fwrite(pFrame->extended_data[0], 1, pFrame->linesize[0], s_outFile);*/
            if (!BufferCopy(pBW, pFrame->extended_data[0], 1, pFrame->linesize[0])) return false;
        }
        else
        {
            for (int s = 0; s < pFrame->nb_samples; ++s)
            {
                for (int c = 0; c < pCodecCtx->ch_layout.nb_channels; ++c)
                {
                    f32 sample = getSample(pCodecCtx, pFrame->extended_data[0], s * pCodecCtx->ch_layout.nb_channels + c);
                    /*fwrite(&sample, sizeof(f32), 1, s_outFile);*/
                    if (!BufferPush(pBW, sample)) return false;
                }
            }
        }
    }

    return true;
}

/**
 * Find the first audio stream and returns its index. If there is no audio stream returns -1.
 */
int
findAudioStream(const AVFormatContext* pFormatCtx)
{
    int audioStreamIndex = -1;
    for (size_t i = 0; i < pFormatCtx->nb_streams; ++i)
    {
        // Use the first audio stream we can find.
        // NOTE: There may be more than one, depending on the file.
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioStreamIndex = i;
            break;
        }
    }
    return audioStreamIndex;
}

/*
 * Print information about the input file and the used codec.
 */
void
printStreamInformation(const AVCodec* pCodec, const AVCodecContext* pCodecCtx, int audioStreamIndex)
{
#ifndef NDEBUG

    LOG("Codec: {}\n", pCodec->long_name);
    if (pCodec->sample_fmts != nullptr)
    {
        LOG("Supported sample formats: ");
        for (int i = 0; pCodec->sample_fmts[i] != -1; ++i)
        {
            CERR("{}", av_get_sample_fmt_name(pCodec->sample_fmts[i]));
            if (pCodec->sample_fmts[i + 1] != -1)
                CERR(", ");
        }
        CERR("\n");
    }
    CERR("---------\n");
    CERR("Stream:        {}\n", audioStreamIndex);
    CERR("Sample Format: {}\n", av_get_sample_fmt_name(pCodecCtx->sample_fmt));
    CERR("Sample Rate:   {}\n", pCodecCtx->sample_rate);
    CERR("Sample Size:   {}\n", av_get_bytes_per_sample(pCodecCtx->sample_fmt));
    CERR("Channels:      {}\n", pCodecCtx->ch_layout.nb_channels);
    CERR("Planar:        {}\n", av_sample_fmt_is_planar(pCodecCtx->sample_fmt));
    CERR(
        "Float Output:  {}\n",
        !RAW_OUT_ON_PLANAR || av_sample_fmt_is_planar(pCodecCtx->sample_fmt) ? "yes" : "no"
    );

#endif
}

/**
 * Receive as many frames as available and handle them.
 */
int
receiveAndHandle(AVCodecContext* pCodecCtx, AVFrame* pFrame, Buffer* pBW)
{
    int err = 0;
    /* Read the packets from the decoder. */
    /* NOTE: Each packet may generate more than one frame, depending on the codec. */
    while ((err = avcodec_receive_frame(pCodecCtx, pFrame)) == 0)
    {
        /* Free any buffers and reset the fields to default values. */
        defer( av_frame_unref(pFrame) );

        if (!handleFrame(pCodecCtx, pFrame, pBW)) return ERROR::DONE;
    }
    return err;
}

void
drainDecoder(AVCodecContext* pCodecCtx, AVFrame* pFrame, Buffer* pBW)
{
    int err = 0;
    /* Some codecs may buffer frames. Sending nullptr activates drain-mode. */
    if ((err = avcodec_send_packet(pCodecCtx, nullptr)) == 0)
    {
        /* Read the remaining packets from the decoder. */
        err = receiveAndHandle(pCodecCtx, pFrame, pBW);
        if (err != AVERROR(EAGAIN) && err != AVERROR_EOF)
        {
            /* Neither EAGAIN nor EOF => Something went wrong. */
            printError("Receive error.", err);
        }
    }
    else
    {
        /* Something went wrong. */
        printError("Send error.", err);
    }
}

ERROR
DecoderOpen(Decoder* s, String sPath)
{
    Arena arena(SIZE_1K);
    defer( ArenaFreeAll(&arena) );

    String sPathNullTerm = StringAlloc(&arena.base, sPath); /* with null char */

    int err = 0;
    err = avformat_open_input(&s->pFormatCtx, sPathNullTerm.pData, {}, {});
    if (err != 0) return ERROR::FILE_OPENING;

    /* In case the file had no header, read some frames and find out which format and codecs are used. */
    /* This does not consume any data. Any read packets are buffered for later use. */
    avformat_find_stream_info(s->pFormatCtx, nullptr);

    /* Try to find an audio stream. */
    s->audioStreamIdx = findAudioStream(s->pFormatCtx);
    if (s->audioStreamIdx == -1)
    {
        avformat_close_input(&s->pFormatCtx);
        return ERROR::AUDIO_STREAM_NOT_FOUND;
    }

    /* Find the correct decoder for the codec. */
    const AVCodec* pCodec = avcodec_find_decoder(s->pFormatCtx->streams[s->audioStreamIdx]->codecpar->codec_id);
    if (!pCodec)
    {
        avformat_close_input(&s->pFormatCtx);
        return ERROR::DECODER_NOT_FOUND;
    }

    /* Initialize codec context for the decoder. */
    s->pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!s->pCodecCtx)
    {
        avformat_close_input(&s->pFormatCtx);
        return ERROR::DECODING_CONTEXT_ALLOCATION;
    }

    /* Fill the codecCtx with the parameters of the codec used in the read file. */
    if ((err = avcodec_parameters_to_context(s->pCodecCtx, s->pFormatCtx->streams[s->audioStreamIdx]->codecpar)) != 0)
    {
        avformat_close_input(&s->pFormatCtx);
        avcodec_free_context(&s->pCodecCtx);
        return ERROR::CODEC_CONTEXT_PARAMETERS;
    }

    /* Explicitly request non planar data. */
    s->pCodecCtx->request_sample_fmt = av_get_alt_sample_fmt(s->pCodecCtx->sample_fmt, 0);

    /* Initialize the decoder. */
    if ((err = avcodec_open2(s->pCodecCtx, pCodec, nullptr)) != 0)
    {
        avformat_close_input(&s->pFormatCtx);
        avcodec_free_context(&s->pCodecCtx);
        return ERROR::INITIALIZING_DECODER;
    }

    printStreamInformation(pCodec, s->pCodecCtx, s->audioStreamIdx);

    s->pFrame = av_frame_alloc();
    if (!s->pFrame)
    {
        avformat_close_input(&s->pFormatCtx);
        avcodec_free_context(&s->pCodecCtx);
        return ERROR::AV_FRAME_ALLOC;
    }

    s->pPacket = av_packet_alloc();

    return ERROR::OK;
}

void
DecoderReadFrames(Decoder* s, f32* pBuff, u32 buffSize)
{
    Buffer bw {pBuff, buffSize, 0};

    /*LOG_NOTIFY("Decoding...\n");*/

    int err = 0;
    while ((err = av_read_frame(s->pFormatCtx, s->pPacket)) != AVERROR_EOF)
    {
        if (err != 0)
        {
            /* Something went wrong. */ 
            printError("Read error.", err);
            break; /* Don't return, so we can clean up nicely. */
        }
        /* Does the packet belong to the correct stream? */
        if (s->pPacket->stream_index != s->audioStreamIdx)
        {
            /* Free the buffers used by the frame and reset all fields. */
            av_packet_unref(s->pPacket);
            continue;
        }
        /* We have a valid packet => send it to the decoder. */
        if ((err = avcodec_send_packet(s->pCodecCtx, s->pPacket)) == 0)
        {
            /* The packet was sent successfully. We don't need it anymore. */
            /* => Free the buffers used by the frame and reset all fields. */
            av_packet_unref(s->pPacket);
        }
        else
        {
            /* Something went wrong. */
            /* EAGAIN is technically no error here but if it occurs we would need to buffer */
            /* the packet and send it again after receiving more frames. Thus we handle it as an error here. */
            printError("Send error.", err);
            break; /* Don't return, so we can clean up nicely. */
        }

        /* Receive and handle frames. */
        /* EAGAIN means we need to send before receiving again. So thats not an error. */
        // if ((err = receiveAndHandle(s->pCodecCtx, s->pFrame, &bw)) != AVERROR(EAGAIN))
        // {
        //     /* Not EAGAIN => Something went wrong. */
        //     printError("Receive error.", err);
        //     break; /* Don't return, so we can clean up nicely. */
        // }

        /* Read the packets from the decoder. */
        /* NOTE: Each packet may generate more than one frame, depending on the codec. */
        while ((err = avcodec_receive_frame(s->pCodecCtx, s->pFrame)) == 0 && bw.idx < bw.buffSize)
        {
            /* Free any buffers and reset the fields to default values. */
            defer( av_frame_unref(s->pFrame) );

            if (!handleFrame(s->pCodecCtx, s->pFrame, &bw)) break;
        }
    }
}

ERROR
openTEST(Decoder* s, String sPath)
{
    String sPathNullTerm = StringAlloc(&inl_OsAllocator.base, sPath); /* with null char */
    defer( StringDestroy(&inl_OsAllocator.base, &sPathNullTerm) );

    int err = 0;
    defer( if (err < 0) DecoderClose(s) );

    err = avformat_open_input(&s->pFormatCtx, sPathNullTerm.pData, {}, {});
    if (err != 0) return ERROR::FILE_OPENING;

    err = avformat_find_stream_info(s->pFormatCtx, {});
    if (err != 0) return ERROR::AUDIO_STREAM_NOT_FOUND;

    LOG("nb_streams: {}\n", s->pFormatCtx->nb_streams);

    int idx = av_find_best_stream(s->pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, {}, 0);
    if (idx < 0) return ERROR::AUDIO_STREAM_NOT_FOUND;

    s->audioStreamIdx = idx;
    s->pStream = s->pFormatCtx->streams[idx];

    const AVCodec* pCodec = avcodec_find_decoder(s->pStream->codecpar->codec_id);

    if (!pCodec) return ERROR::DECODER_NOT_FOUND;

    LOG_NOTIFY("codec name: '{}'\n", pCodec->long_name);

    s->pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!s->pCodecCtx) return ERROR::DECODING_CONTEXT_ALLOCATION;

    avcodec_parameters_to_context(s->pCodecCtx, s->pStream->codecpar);

    err = avcodec_open2(s->pCodecCtx, pCodec, {});
    if (err < 0)
    {
        LOG("avcodec_open2\n");
        return ERROR::UNKNOWN;
    }

    s->pPacket = av_packet_alloc();
    s->pFrame = av_frame_alloc();

    /*s->pFifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLT, s->pStream->codecpar->ch_layout.nb_channels, 1);*/
    /*defer( av_audio_fifo_free(s->pFifo) );*/

    return ERROR::OK;
}

ERROR
writeBufferTEST(Decoder* s, f32* pBuff, u32 buffSize, u32 nFrames, u32* pDataWritten)
{
    int err = 0;

    SwrContext* pResampler = nullptr;
    err = swr_alloc_set_opts2(&pResampler,
        &s->pStream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, 48000,
        &s->pStream->codecpar->ch_layout, (AVSampleFormat)s->pStream->codecpar->format, s->pStream->codecpar->sample_rate,
        0, {}
    );
    defer( swr_free(&pResampler) );

    u32 nf = 0;
    while (av_read_frame(s->pFormatCtx, s->pPacket) == 0)
    {
        if (s->pPacket->stream_index != s->pStream->index) continue;
        err = avcodec_send_packet(s->pCodecCtx, s->pPacket);
        if (err != 0 && err != AVERROR(EAGAIN))
            LOG_WARN("!EAGAIN\n");

        while ((err = avcodec_receive_frame(s->pCodecCtx, s->pFrame)) == 0)
        {
            // defer( av_frame_unref(s->pFrame) );

            const auto f = s->pFrame;
            const auto pRes = s->pFrame;

            // AVFrame* pRes = av_frame_alloc();
            // defer( av_frame_free(&pRes) );

            // pRes->sample_rate = f->sample_rate;
            // pRes->ch_layout = f->ch_layout;
            // /*pRes->format = f->format;*/
            // pRes->format = AV_SAMPLE_FMT_FLT;
            // /*LOG("AV_SAMPLE_FMT_FLT: {}, f.format: {}\n", (int)AV_SAMPLE_FMT_FLT, f->format);*/

            // err = swr_convert_frame(pResampler, pRes, s->pFrame);
            // if (err != 0)
            // {
            //     LOG_WARN("swr_convert_frame\n");
            //     return ERROR::UNKNOWN;
            // }

            // if (nf >= buffSize)
            // {
            //     /*LOG("LEAVE\n");*/
            //     *pDataWritten += nf;
            //     return ERROR::OK;
            // }

            // auto dataSize = av_samples_get_buffer_size({}, pRes->ch_layout.nb_channels, pRes->nb_samples, (AVSampleFormat)pRes->format, 0);
            // memcpy(pBuff + nf, pRes->data[0], dataSize);
            // nf += dataSize/4;

            /*LOG("dataSize: {}, nb_samples: {}, nf: {}\n", dataSize, pRes->nb_samples*2, nf*4);*/

            for (int i = 0; i < pRes->nb_samples; ++i)
            {
                for (int ch = 0; ch < pRes->ch_layout.nb_channels; ++ch)
                {
                    if (nf >= buffSize)
                    {
                        /*LOG("LEAVE\n");*/
                        *pDataWritten += nf;
                        return ERROR::OK;
                    }

                    /*f32 s = *(f32*)(pRes->data[ch] + dataSize*i);*/
                    f32 s = ((f32*)pRes->data[ch])[i];
                    pBuff[nf++] = s;
                }
            }

            // for (int ch = 0; ch < pRes->ch_layout.nb_channels; ++ch)
            // {
            //     for (int i = 0; i < pRes->nb_samples; ++i)
            //     {
            //         if (nf >= buffSize)
            //         {
            //             LOG("LEAVE\n");
            //             *pDataWritten += nf;
            //             return ERROR::OK;
            //         }

            //         /*f32 s = *(f32*)(pRes->data[ch] + dataSize*i);*/
            //         f32 s = ((f32*)pRes->data[ch])[i];
            //         pBuff[nf++] = s;
            //     }
            // }
        }
    }

    return ERROR::OK;
}

int
test(int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("Usage: decode <audofile>\n");
        return 1;
    }

    // Get the filename.
    char* filename = argv[1];
    // Open the outfile called "<infile>.raw".
    char* outFilename = (char*)malloc(strlen(filename) + 5);
    defer( free(outFilename) );

    strcpy(outFilename, filename);
    strcpy(outFilename + strlen(filename), ".raw");
    /*s_outFile = fopen(outFilename, "w+");*/

    /*if (s_outFile == nullptr)*/
    /*    fprintf(stderr, "Unable to open output file \"%s\".\n", outFilename);*/

    /*defer( fclose(s_outFile) );*/

    int err = 0;
    AVFormatContext* pFormatCtx = nullptr;
    // Open the file and read the header.
    if ((err = avformat_open_input(&pFormatCtx, filename, nullptr, 0)) != 0)
        return printError("Error opening file.", err);

    defer( avformat_close_input(&pFormatCtx) );

    // In case the file had no header, read some frames and find out which format and codecs are used.
    // This does not consume any data. Any read packets are buffered for later use.
    avformat_find_stream_info(pFormatCtx, nullptr);

    // Try to find an audio stream.
    int audioStreamIndex = findAudioStream(pFormatCtx);
    if (audioStreamIndex == -1)
    {
        // No audio stream was found.
        fprintf(stderr, "None of the available %d streams are audio streams.\n", pFormatCtx->nb_streams);
        return -1;
    }

    // Find the correct decoder for the codec.
    const AVCodec* pCodec = avcodec_find_decoder(pFormatCtx->streams[audioStreamIndex]->codecpar->codec_id);
    if (pCodec == nullptr)
    {
        // Decoder not found.
        fprintf(stderr, "Decoder not found. The codec is not supported.\n");
        return -1;
    }

    // Initialize codec context for the decoder.
    AVCodecContext* pCodecCtx = avcodec_alloc_context3(pCodec);
    if (pCodecCtx == nullptr)
    {
        // Something went wrong. Cleaning up...
        fprintf(stderr, "Could not allocate a decoding context.\n");
        return -1;
    }
    defer( avcodec_free_context(&pCodecCtx) );

    // Fill the codecCtx with the parameters of the codec used in the read file.
    if ((err = avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[audioStreamIndex]->codecpar)) != 0)
    {
        // Something went wrong. Cleaning up...
        return printError("Error setting codec context parameters.", err);
    }

    // Explicitly request non planar data.
    pCodecCtx->request_sample_fmt = av_get_alt_sample_fmt(pCodecCtx->sample_fmt, 0);

    // Initialize the decoder.
    if ((err = avcodec_open2(pCodecCtx, pCodec, nullptr)) != 0)
        return -1;

    // Print some intersting file information.
    printStreamInformation(pCodec, pCodecCtx, audioStreamIndex);

    AVFrame* pFrame = av_frame_alloc();
    if (pFrame == nullptr)
        return -1;
    defer( av_frame_free(&pFrame) );

    // Prepare the packet.
    AVPacket* pPacket = av_packet_alloc();
    defer( av_packet_free(&pPacket) );

    while ((err = av_read_frame(pFormatCtx, pPacket)) != AVERROR_EOF)
    {
        if (err != 0)
        {
            // Something went wrong.
            printError("Read error.", err);
            break; // Don't return, so we can clean up nicely.
        }
        // Does the packet belong to the correct stream?
        if (pPacket->stream_index != audioStreamIndex)
        {
            // Free the buffers used by the frame and reset all fields.
            av_packet_unref(pPacket);
            continue;
        }
        // We have a valid packet => send it to the decoder.
        if ((err = avcodec_send_packet(pCodecCtx, pPacket)) == 0)
        {
            // The packet was sent successfully. We don't need it anymore.
            // => Free the buffers used by the frame and reset all fields.
            av_packet_unref(pPacket);
        }
        else
        {
            // Something went wrong.
            // EAGAIN is technically no error here but if it occurs we would need to buffer
            // the packet and send it again after receiving more frames. Thus we handle it as an error here.
            printError("Send error.", err);
            break; // Don't return, so we can clean up nicely.
        }

        // Receive and handle frames.
        // EAGAIN means we need to send before receiving again. So thats not an error.
        // if ((err = receiveAndHandle(pCodecCtx, pFrame)) != AVERROR(EAGAIN))
        // {
        //     // Not EAGAIN => Something went wrong.
        //     printError("Receive error.", err);
        //     break; // Don't return, so we can clean up nicely.
        // }
    }

    // Drain the decoder.
    // drainDecoder(pCodecCtx, pFrame);

    return 0;
}

} /* namespace ffmpeg */

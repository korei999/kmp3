#include "ffmpeg.hh"

#include "adt/Arena.hh"
#include "adt/String.hh"
#include "adt/defer.hh"
#include "adt/types.hh"

using namespace adt;

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

/* https://gist.github.com/jcelerier/c28310e34c189d37e99609be8cd74bdcG*/
#define RAW_OUT_ON_PLANAR true

namespace ffmpeg
{

static FILE* s_outFile;

/**
 * Print an error string describing the errorCode to stderr.
 */
int
printError(const char* prefix, int errorCode)
{
    if (errorCode == 0)
    {
        return 0;
    }
    else
    {
        const size_t bufsize = 64;
        char buf[bufsize];

        if (av_strerror(errorCode, buf, bufsize) != 0)
        {
            strcpy(buf, "UNKNOWN_ERROR");
        }
        fprintf(stderr, "%s (%d: %s)\n", prefix, errorCode, buf);

        return errorCode;
    }
}

/**
 * Extract a single sample and convert to f32.
 */
f32
getSample(const AVCodecContext* codecCtx, u8* buffer, int sampleIndex)
{
    s64 val = 0;
    f32 ret = 0;
    int sampleSize = av_get_bytes_per_sample(codecCtx->sample_fmt);
    switch (sampleSize)
    {
        case 1:
            // 8bit samples are always unsigned
            val = ((u8*)buffer)[sampleIndex];
            // make signed
            val -= 127;
            break;

        case 2:
            val = ((s16*)buffer)[sampleIndex];
            break;

        case 4:
            val = ((s32*)buffer)[sampleIndex];
            break;

        case 8:
            val = ((s64*)buffer)[sampleIndex];
            break;

        default:
            fprintf(stderr, "Invalid sample size %d.\n", sampleSize);
            return 0;
    }

    // Check which data type is in the sample.
    switch (codecCtx->sample_fmt)
    {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_U8P:
        case AV_SAMPLE_FMT_S16P:
        case AV_SAMPLE_FMT_S32P:
            // integer => Scale to [-1, 1] and convert to f32.
            ret = f32(val) / f32((1 << (sampleSize*8 - 1)) - 1);
            break;

        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            // f32 => reinterpret
            ret = *(f32*)&val;
            break;

        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            // double => reinterpret and then static cast down
            ret = *(f64*)&val;
            break;

        default:
            fprintf(stderr, "Invalid sample format %s.\n", av_get_sample_fmt_name(codecCtx->sample_fmt));
            return 0;
    }

    return ret;
}

/**
 * Write the frame to an output file.
 */
void
handleFrame(const AVCodecContext* codecCtx, const AVFrame* frame)
{
    if (av_sample_fmt_is_planar(codecCtx->sample_fmt) == 1)
    {
        // This means that the data of each channel is in its own buffer.
        // => frame->extended_data[i] contains data for the i-th channel.
        for (int s = 0; s < frame->nb_samples; ++s)
        {
            for (int c = 0; c < codecCtx->ch_layout.nb_channels; ++c)
            {
                f32 sample = getSample(codecCtx, frame->extended_data[c], s);
                fwrite(&sample, sizeof(f32), 1, s_outFile);
            }
        }
    }
    else
    {
        // This means that the data of each channel is in the same buffer.
        // => frame->extended_data[0] contains data of all channels.
        if (RAW_OUT_ON_PLANAR)
        {
            fwrite(frame->extended_data[0], 1, frame->linesize[0], s_outFile);
        }
        else
        {
            for (int s = 0; s < frame->nb_samples; ++s)
            {
                for (int c = 0; c < codecCtx->ch_layout.nb_channels; ++c)
                {
                    f32 sample =
                        getSample(codecCtx, frame->extended_data[0], s * codecCtx->ch_layout.nb_channels + c);
                    fwrite(&sample, sizeof(f32), 1, s_outFile);
                }
            }
        }
    }
}

/**
 * Find the first audio stream and returns its index. If there is no audio stream returns -1.
 */
int
findAudioStream(const AVFormatContext* formatCtx)
{
    int audioStreamIndex = -1;
    for (size_t i = 0; i < formatCtx->nb_streams; ++i)
    {
        // Use the first audio stream we can find.
        // NOTE: There may be more than one, depending on the file.
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
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
printStreamInformation(const AVCodec* codec, const AVCodecContext* codecCtx, int audioStreamIndex)
{
    fprintf(stderr, "Codec: %s\n", codec->long_name);
    if (codec->sample_fmts != nullptr)
    {
        fprintf(stderr, "Supported sample formats: ");
        for (int i = 0; codec->sample_fmts[i] != -1; ++i)
        {
            fprintf(stderr, "%s", av_get_sample_fmt_name(codec->sample_fmts[i]));
            if (codec->sample_fmts[i + 1] != -1)
            {
                fprintf(stderr, ", ");
            }
        }
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "---------\n");
    fprintf(stderr, "Stream:        %7d\n", audioStreamIndex);
    fprintf(stderr, "Sample Format: %7s\n", av_get_sample_fmt_name(codecCtx->sample_fmt));
    fprintf(stderr, "Sample Rate:   %7d\n", codecCtx->sample_rate);
    fprintf(stderr, "Sample Size:   %7d\n", av_get_bytes_per_sample(codecCtx->sample_fmt));
    fprintf(stderr, "Channels:      %7d\n", codecCtx->ch_layout.nb_channels);
    fprintf(stderr, "Planar:        %7d\n", av_sample_fmt_is_planar(codecCtx->sample_fmt));
    fprintf(
        stderr, "Float Output:  %7s\n",
        !RAW_OUT_ON_PLANAR || av_sample_fmt_is_planar(codecCtx->sample_fmt) ? "yes" : "no"
    );
}

/**
 * Receive as many frames as available and handle them.
 */
int
receiveAndHandle(AVCodecContext* codecCtx, AVFrame* frame)
{
    int err = 0;
    // Read the packets from the decoder.
    // NOTE: Each packet may generate more than one frame, depending on the codec.
    while ((err = avcodec_receive_frame(codecCtx, frame)) == 0)
    {
        // Let's handle the frame in a function.
        handleFrame(codecCtx, frame);
        // Free any buffers and reset the fields to default values.
        av_frame_unref(frame);
    }
    return err;
}

void
drainDecoder(AVCodecContext* codecCtx, AVFrame* frame)
{
    int err = 0;
    // Some codecs may buffer frames. Sending nullptr activates drain-mode.
    if ((err = avcodec_send_packet(codecCtx, nullptr)) == 0)
    {
        // Read the remaining packets from the decoder.
        err = receiveAndHandle(codecCtx, frame);
        if (err != AVERROR(EAGAIN) && err != AVERROR_EOF)
        {
            // Neither EAGAIN nor EOF => Something went wrong.
            printError("Receive error.", err);
        }
    }
    else
    {
        // Something went wrong.
        printError("Send error.", err);
    }
}

bool
openFile(String path)
{
    Arena arena(SIZE_1K);
    defer( ArenaFreeAll(&arena) );

    String sPath = StringAlloc(&arena.base, path); /* with null char */

    int err = 0;
    /*AVFormatContext* formatCtx = avformat_open_input(&formatCtx, filename, nullptr, 0);*/

    /*if ((err = avformat_open_input(&formatCtx, filename, nullptr, 0)) != 0)*/
    /*    return printError("Error opening file.", err);*/


    return true;
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
    s_outFile = fopen(outFilename, "w+");

    if (s_outFile == nullptr)
        fprintf(stderr, "Unable to open output file \"%s\".\n", outFilename);

    defer( fclose(s_outFile) );

    // Initialize the libavformat. This registers all muxers, demuxers and protocols.
    /*av_register_all();*/

    int err = 0;
    AVFormatContext* formatCtx = nullptr;
    // Open the file and read the header.
    if ((err = avformat_open_input(&formatCtx, filename, nullptr, 0)) != 0)
        return printError("Error opening file.", err);

    defer( avformat_close_input(&formatCtx) );

    // In case the file had no header, read some frames and find out which format and codecs are used.
    // This does not consume any data. Any read packets are buffered for later use.
    avformat_find_stream_info(formatCtx, nullptr);

    // Try to find an audio stream.
    int audioStreamIndex = findAudioStream(formatCtx);
    if (audioStreamIndex == -1)
    {
        // No audio stream was found.
        fprintf(stderr, "None of the available %d streams are audio streams.\n", formatCtx->nb_streams);
        return -1;
    }

    // Find the correct decoder for the codec.
    const AVCodec* codec = avcodec_find_decoder(formatCtx->streams[audioStreamIndex]->codecpar->codec_id);
    if (codec == nullptr)
    {
        // Decoder not found.
        fprintf(stderr, "Decoder not found. The codec is not supported.\n");
        return -1;
    }

    // Initialize codec context for the decoder.
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (codecCtx == nullptr)
    {
        // Something went wrong. Cleaning up...
        fprintf(stderr, "Could not allocate a decoding context.\n");
        return -1;
    }
    defer( avcodec_free_context(&codecCtx) );

    // Fill the codecCtx with the parameters of the codec used in the read file.
    if ((err = avcodec_parameters_to_context(codecCtx, formatCtx->streams[audioStreamIndex]->codecpar)) != 0)
    {
        // Something went wrong. Cleaning up...
        return printError("Error setting codec context parameters.", err);
    }

    // Explicitly request non planar data.
    codecCtx->request_sample_fmt = av_get_alt_sample_fmt(codecCtx->sample_fmt, 0);

    // Initialize the decoder.
    if ((err = avcodec_open2(codecCtx, codec, nullptr)) != 0)
        return -1;

    // Print some intersting file information.
    printStreamInformation(codec, codecCtx, audioStreamIndex);

    AVFrame* frame = av_frame_alloc();
    if (frame == nullptr)
        return -1;
    defer( av_frame_free(&frame) );

    // Prepare the packet.
    AVPacket packet {};
    while ((err = av_read_frame(formatCtx, &packet)) != AVERROR_EOF)
    {
        if (err != 0)
        {
            // Something went wrong.
            printError("Read error.", err);
            break; // Don't return, so we can clean up nicely.
        }
        // Does the packet belong to the correct stream?
        if (packet.stream_index != audioStreamIndex)
        {
            // Free the buffers used by the frame and reset all fields.
            av_packet_unref(&packet);
            continue;
        }
        // We have a valid packet => send it to the decoder.
        if ((err = avcodec_send_packet(codecCtx, &packet)) == 0)
        {
            // The packet was sent successfully. We don't need it anymore.
            // => Free the buffers used by the frame and reset all fields.
            av_packet_unref(&packet);
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
        if ((err = receiveAndHandle(codecCtx, frame)) != AVERROR(EAGAIN))
        {
            // Not EAGAIN => Something went wrong.
            printError("Receive error.", err);
            break; // Don't return, so we can clean up nicely.
        }
    }

    // Drain the decoder.
    drainDecoder(codecCtx, frame);

    return 0;
}

} /* namespace ffmpeg */

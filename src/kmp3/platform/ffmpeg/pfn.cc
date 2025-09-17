#include "pfn.hh"

#include <dlfcn.h>

using namespace adt;

namespace platform::ffmpeg::pfn
{

void (*avformat_close_input)(AVFormatContext** s);
void (*av_packet_free)(AVPacket** pkt);
void (*av_frame_free)(AVFrame** frame);

AVDictionaryEntry* (*av_dict_get)(const AVDictionary* m, const char* key, const AVDictionaryEntry* prev, int flags);

void (*avcodec_free_context)(AVCodecContext** avctx);
const AVCodec* (*avcodec_find_decoder)(enum AVCodecID id);
AVCodecContext* (*avcodec_alloc_context3)(const AVCodec* codec);

int (*avcodec_parameters_to_context)(AVCodecContext* codec, const struct AVCodecParameters* par);
int (*avcodec_open2)(AVCodecContext* avctx, const AVCodec* codec, AVDictionary** options);

AVPacket* (*av_packet_alloc)(void);
AVFrame* (*av_frame_alloc)(void);
int (*av_read_frame)(AVFormatContext* s, AVPacket* pkt);
int (*avcodec_send_packet)(AVCodecContext* avctx, const AVPacket* avpkt);
int (*avcodec_receive_frame)(AVCodecContext* avctx, AVFrame* frame);

int (*swr_alloc_set_opts2)(
    struct SwrContext** ps, const AVChannelLayout* out_ch_layout, enum AVSampleFormat out_sample_fmt,
    int out_sample_rate, const AVChannelLayout* in_ch_layout, enum AVSampleFormat in_sample_fmt, int in_sample_rate,
    int log_offset, void* log_ctx
);
int (*swr_config_frame)(SwrContext* swr, const AVFrame* out, const AVFrame* in);
void (*swr_free)(struct SwrContext** s);
int (*swr_convert_frame)(SwrContext* swr, AVFrame* output, const AVFrame* input);

int (*av_image_fill_linesizes)(int linesizes[4], enum AVPixelFormat pix_fmt, int width);
int (*av_frame_get_buffer)(AVFrame* frame, int align);

void (*av_log_set_level)(int level);
int (*avformat_open_input)( AVFormatContext** ps, const char* url, const AVInputFormat* fmt, AVDictionary** options);
int (*avformat_find_stream_info)(AVFormatContext* ic, AVDictionary** options);

int (*av_find_best_stream)(
    AVFormatContext* ic, enum AVMediaType type, int wanted_stream_nb, int related_stream,
    const struct AVCodec** decoder_ret, int flags
);

void (*avcodec_flush_buffers)(AVCodecContext* avctx);
int64_t (*av_rescale_q)(int64_t a, AVRational bq, AVRational cq) av_const;
int (*av_seek_frame)(AVFormatContext* s, int stream_index, int64_t timestamp, int flags);
void (*av_packet_unref)(AVPacket* pkt);

void (*av_frame_unref)(AVFrame* frame);
int (*av_strerror)(int errnum, char *errbuf, size_t errbuf_size);

#ifdef OPT_CHAFA

struct SwsContext* (*sws_getContext)(
    int srcW, int srcH, enum AVPixelFormat srcFormat, int dstW, int dstH, enum AVPixelFormat dstFormat, int flags,
    SwsFilter* srcFilter, SwsFilter* dstFilter, const double* param
);
int (*sws_scale_frame)(struct SwsContext* c, AVFrame* dst, const AVFrame* src);
void (*sws_freeContext)(struct SwsContext* swsContext);

#endif /* OPT_CHAFA */

static void* s_pLibavformat;
static void* s_pLiblibswscale;

bool
loadSO()
{
#define SYM(lib, name)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        name = (decltype(name))dlsym(lib, #name);                                                                      \
        if (!name)                                                                                                     \
        {                                                                                                              \
            LogDebug {"failed to load: '{}'\n", #name};                                                                \
            dlclose(lib);                                                                                              \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)

    {
#ifdef __APPLE__
        s_pLibavformat = dlopen("/usr/local/lib/libavformat.dylib", RTLD_NOW | RTLD_LOCAL);
#else
        s_pLibavformat = dlopen("libavformat.so", RTLD_NOW | RTLD_LOCAL);
#endif
        if (!s_pLibavformat) return false;

        SYM(s_pLibavformat, avformat_close_input);
        SYM(s_pLibavformat, av_packet_free);
        SYM(s_pLibavformat, av_frame_free);
        SYM(s_pLibavformat, av_dict_get);

        SYM(s_pLibavformat, avcodec_free_context);
        SYM(s_pLibavformat, avcodec_find_decoder);
        SYM(s_pLibavformat, avcodec_alloc_context3);

        SYM(s_pLibavformat, avcodec_parameters_to_context);
        SYM(s_pLibavformat, avcodec_open2);

        SYM(s_pLibavformat, av_packet_alloc);
        SYM(s_pLibavformat, av_frame_alloc);
        SYM(s_pLibavformat, av_read_frame);
        SYM(s_pLibavformat, avcodec_send_packet);
        SYM(s_pLibavformat, avcodec_receive_frame);

        SYM(s_pLibavformat, swr_alloc_set_opts2);
        SYM(s_pLibavformat, swr_config_frame);
        SYM(s_pLibavformat, swr_free);
        SYM(s_pLibavformat, swr_convert_frame);

        SYM(s_pLibavformat, av_image_fill_linesizes);
        SYM(s_pLibavformat, av_frame_get_buffer);

        SYM(s_pLibavformat, av_log_set_level);
        SYM(s_pLibavformat, avformat_open_input);
        SYM(s_pLibavformat, avformat_find_stream_info);
        SYM(s_pLibavformat, av_find_best_stream);

        SYM(s_pLibavformat, avcodec_flush_buffers);
        SYM(s_pLibavformat, av_rescale_q);
        SYM(s_pLibavformat, av_seek_frame);
        SYM(s_pLibavformat, av_packet_unref);

        SYM(s_pLibavformat, av_frame_unref);
        SYM(s_pLibavformat, av_strerror);
    }

    {
#ifdef OPT_CHAFA

#ifdef __APPLE__
        s_pLiblibswscale = dlopen("/usr/local/lib/libswscale.dylib", RTLD_NOW | RTLD_LOCAL);
#else
        s_pLiblibswscale = dlopen("libswscale.so", RTLD_NOW | RTLD_LOCAL);
#endif
        if (!s_pLiblibswscale) return false;

        SYM(s_pLiblibswscale, sws_getContext);
        SYM(s_pLiblibswscale, sws_scale_frame);
        SYM(s_pLiblibswscale, sws_freeContext);
#endif
    }

    return true;

#undef SYM
}

void
unloadSO()
{
    if (s_pLibavformat) dlclose(s_pLibavformat);
#ifdef OPT_CHAFA
    if (s_pLiblibswscale) dlclose(s_pLiblibswscale);
#endif
}

} /* namespace platform::ffmpeg::pfn */

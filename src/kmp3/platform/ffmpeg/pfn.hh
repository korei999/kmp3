#pragma once


extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#ifdef OPT_CHAFA
    #include <libswscale/swscale.h>
#endif
}

namespace platform::ffmpeg::pfn
{

bool loadSO();

extern void (*avformat_close_input)(AVFormatContext** s);
extern void (*avcodec_free_context)(AVCodecContext** avctx);
extern void (*av_packet_free)(AVPacket** pkt);
extern void (*av_frame_free)(AVFrame** frame);

extern AVDictionaryEntry* (*av_dict_get)(const AVDictionary* m, const char* key, const AVDictionaryEntry* prev, int flags);

extern const AVCodec* (*avcodec_find_decoder)(enum AVCodecID id);
extern AVCodecContext* (*avcodec_alloc_context3)(const AVCodec* codec);

extern int (*avcodec_parameters_to_context)(AVCodecContext* codec, const struct AVCodecParameters* par);
extern int (*avcodec_open2)(AVCodecContext* avctx, const AVCodec* codec, AVDictionary** options);

extern AVPacket* (*av_packet_alloc)(void);
extern AVFrame* (*av_frame_alloc)(void);
extern int (*av_read_frame)(AVFormatContext* s, AVPacket* pkt);
extern int (*avcodec_send_packet)(AVCodecContext* avctx, const AVPacket* avpkt);
extern int (*avcodec_receive_frame)(AVCodecContext* avctx, AVFrame* frame);

extern int (*swr_alloc_set_opts2)(
    struct SwrContext** ps, const AVChannelLayout* out_ch_layout, enum AVSampleFormat out_sample_fmt,
    int out_sample_rate, const AVChannelLayout* in_ch_layout, enum AVSampleFormat in_sample_fmt, int in_sample_rate,
    int log_offset, void* log_ctx
);
extern int (*swr_config_frame)(SwrContext* swr, const AVFrame* out, const AVFrame* in);
extern void (*swr_free)(struct SwrContext** s);
extern int (*swr_convert_frame)(SwrContext* swr, AVFrame* output, const AVFrame* input);

extern int (*av_image_fill_linesizes)(int linesizes[4], enum AVPixelFormat pix_fmt, int width);
extern int (*av_frame_get_buffer)(AVFrame* frame, int align);

extern void (*av_log_set_level)(int level);
extern int (*avformat_open_input)( AVFormatContext** ps, const char* url, const AVInputFormat* fmt, AVDictionary** options);
extern int (*avformat_find_stream_info)(AVFormatContext* ic, AVDictionary** options);

extern int (*av_find_best_stream)(
    AVFormatContext* ic, enum AVMediaType type, int wanted_stream_nb, int related_stream,
    const struct AVCodec** decoder_ret, int flags
);

extern void (*avcodec_flush_buffers)(AVCodecContext* avctx);
extern int64_t (*av_rescale_q)(int64_t a, AVRational bq, AVRational cq) av_const;
extern int (*av_seek_frame)(AVFormatContext* s, int stream_index, int64_t timestamp, int flags);
extern void (*av_packet_unref)(AVPacket* pkt);

extern void (*av_frame_unref)(AVFrame* frame);
extern int (*av_strerror)(int errnum, char *errbuf, size_t errbuf_size);

#ifdef OPT_CHAFA

extern struct SwsContext* (*sws_getContext)(
    int srcW, int srcH, enum AVPixelFormat srcFormat, int dstW, int dstH, enum AVPixelFormat dstFormat, int flags,
    SwsFilter* srcFilter, SwsFilter* dstFilter, const double* param
);
extern int (*sws_scale_frame)(struct SwsContext* c, AVFrame* dst, const AVFrame* src);
extern void (*sws_freeContext)(struct SwsContext* swsContext);

#endif /* OPT_CHAFA */
} /* namespace platform::ffmpeg::pfn */

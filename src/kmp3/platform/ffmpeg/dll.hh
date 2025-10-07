#pragma once

extern "C"
{

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>

#ifdef OPT_CHAFA
    #include <libswscale/swscale.h>
#endif

}

namespace platform::ffmpeg::dll
{

bool loadLibs();
void unloadLibs();

#define PFN_INLINE(name) inline std::add_pointer_t<decltype(::name)> name;

PFN_INLINE(avformat_close_input);
PFN_INLINE(av_packet_free);
PFN_INLINE(av_frame_free);

PFN_INLINE(av_dict_get);

PFN_INLINE(avcodec_free_context);
PFN_INLINE(avcodec_find_decoder);
PFN_INLINE(avcodec_alloc_context3);

PFN_INLINE(avcodec_parameters_to_context);
PFN_INLINE(avcodec_open2);

PFN_INLINE(av_packet_alloc);
PFN_INLINE(av_frame_alloc);
PFN_INLINE(av_read_frame);
PFN_INLINE(avcodec_send_packet);
PFN_INLINE(avcodec_receive_frame);

PFN_INLINE(swr_alloc_set_opts2);
PFN_INLINE(swr_config_frame);
PFN_INLINE(swr_free);
PFN_INLINE(swr_convert_frame);

PFN_INLINE(av_image_fill_linesizes);
PFN_INLINE(av_frame_get_buffer);

PFN_INLINE(av_log_set_level);
PFN_INLINE(avformat_open_input);
PFN_INLINE(avformat_find_stream_info);

PFN_INLINE(av_find_best_stream);

PFN_INLINE(avcodec_flush_buffers);
PFN_INLINE(av_rescale_q);
PFN_INLINE(av_seek_frame);
PFN_INLINE(av_packet_unref);

PFN_INLINE(av_frame_unref);
PFN_INLINE(av_strerror);

#ifdef OPT_CHAFA

PFN_INLINE(sws_getContext);
PFN_INLINE(sws_scale_frame);
PFN_INLINE(sws_freeContext);

#endif /* OPT_CHAFA */

#undef PFN_INLINE

} /* namespace platform::ffmpeg::dll */

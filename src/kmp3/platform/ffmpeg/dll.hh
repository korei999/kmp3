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

#include "adt/SOA.hh" /* PP_FOR_EACH macros. */

namespace platform::ffmpeg::dll
{

bool loadLibs();
void unloadLibs();

#define PLATFORM_FFMPEG_DLL_PFN_EXTERN(FUNC) extern decltype(::FUNC)* FUNC;

#define PLATFORM_FFMPEG_DLL_CORE_PFN_LIST \
    avformat_close_input,\
    av_packet_free,\
    av_frame_free,\
\
    av_dict_get,\
\
    avcodec_free_context,\
    avcodec_find_decoder,\
    avcodec_alloc_context3,\
\
    avcodec_parameters_to_context,\
    avcodec_open2,\
\
    av_packet_alloc,\
    av_frame_alloc,\
    av_read_frame,\
    avcodec_send_packet,\
    avcodec_receive_frame,\
\
    swr_alloc_set_opts2,\
    swr_config_frame,\
    swr_free,\
    swr_convert_frame,\
\
    av_image_fill_linesizes,\
    av_frame_get_buffer,\
\
    av_log_set_level,\
    avformat_open_input,\
    avformat_find_stream_info,\
\
    av_find_best_stream,\
\
    avcodec_flush_buffers,\
    av_rescale_q,\
    av_seek_frame,\
    av_packet_unref,\
\
    av_frame_unref,\
    av_strerror

ADT_PP_FOR_EACH(PLATFORM_FFMPEG_DLL_PFN_EXTERN, PLATFORM_FFMPEG_DLL_CORE_PFN_LIST)

#ifdef OPT_CHAFA

#define PLATFORM_FFMPEG_DLL_CHAFA_PFN_LIST \
    sws_getContext,\
    sws_scale_frame,\
    sws_freeContext

ADT_PP_FOR_EACH(PLATFORM_FFMPEG_DLL_PFN_EXTERN, PLATFORM_FFMPEG_DLL_CHAFA_PFN_LIST)

#endif

} /* namespace platform::ffmpeg::dll */

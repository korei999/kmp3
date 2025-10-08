#include "dll.hh"

#include <dlfcn.h>

using namespace adt;

namespace platform::ffmpeg::dll
{

static void* s_pLibavformat;
static void* s_pLiblibswscale;

static void*
tryLoad(const StringView svDLLName)
{
    constexpr StringView aPaths[] {
        "",
        "/usr/local/lib/",
        "/usr/lib/",
        "/opt/local/lib/",
        "/opt/homebrew/lib/",
    };

    IThreadPool::ArenaType* pArena = IThreadPool::inst()->arena();

    for (isize i = 0; i < utils::size(aPaths); ++i)
    {
        IArena::Scope arenaScope {pArena};

        String s = StringCat(pArena, aPaths[i], svDLLName);
        void* pRet = dlopen(s.data(), RTLD_NOW | RTLD_LOCAL);
        if (pRet != nullptr) return pRet;
    }

    return nullptr;
}

bool
loadLibs()
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
        s_pLibavformat = tryLoad("libavformat.dylib");
#else
        s_pLibavformat = tryLoad("libavformat.so");
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
        s_pLiblibswscale = tryLoad("libswscale.dylib");
#else
        s_pLiblibswscale = tryLoad("libswscale.so");
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
unloadLibs()
{
    if (s_pLibavformat) dlclose(s_pLibavformat), s_pLibavformat = nullptr;
#ifdef OPT_CHAFA
    if (s_pLiblibswscale) dlclose(s_pLiblibswscale), s_pLiblibswscale = nullptr;
#endif
}

} /* namespace platform::ffmpeg::dll */

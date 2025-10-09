#include "dll.hh"

#include <dlfcn.h>

using namespace adt;

namespace platform::ffmpeg::dll
{

#define PLATFORM_FFMPEG_DLL_PFN(FUNC) decltype(::FUNC)* FUNC;

ADT_PP_FOR_EACH(PLATFORM_FFMPEG_DLL_PFN,
    PLATFORM_FFMPEG_DLL_CORE_PFN_LIST
)
static void* s_pLibavformat;

#ifdef OPT_CHAFA

ADT_PP_FOR_EACH(PLATFORM_FFMPEG_DLL_PFN,
    PLATFORM_FFMPEG_DLL_CHAFA_PFN_LIST
)
static void* s_pLiblibswscale;

#endif

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

#define _SYM(FUNC) SYM(s_pLibavformat, FUNC);
        ADT_PP_FOR_EACH(_SYM, PLATFORM_FFMPEG_DLL_CORE_PFN_LIST);
#undef _SYM
    }

    {
#ifdef OPT_CHAFA

#ifdef __APPLE__
        s_pLiblibswscale = tryLoad("libswscale.dylib");
#else
        s_pLiblibswscale = tryLoad("libswscale.so");
#endif
        if (!s_pLiblibswscale) return false;

#define _SYM(FUNC) SYM(s_pLiblibswscale, FUNC);
        ADT_PP_FOR_EACH(_SYM, PLATFORM_FFMPEG_DLL_CHAFA_PFN_LIST);
#undef _SYM

#endif
    }

    return true;
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

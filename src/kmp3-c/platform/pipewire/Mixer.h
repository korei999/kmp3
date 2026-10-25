#include "audio.h"

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-field-initializers"
    #pragma clang diagnostic ignored "-Wpedantic"
#endif

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    #pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

typedef struct platform_pipewire_Mixer platform_pipewire_Mixer;
struct platform_pipewire_Mixer
{
    union
    {
        audio_IMixer;
        audio_IMixer base;
    };

    enum spa_audio_format eformat;

    struct pw_thread_loop* pThrdLoop;
    struct pw_stream* pStream;
};

platform_pipewire_Mixer platform_pipewire_MixerCreate(void);

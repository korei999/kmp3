# src/kmp3/platform/CMakeLists.txt

add_subdirectory(ansi)
add_subdirectory(ffmpeg)

if (OPT_ALSA)
    add_subdirectory(alsa)
endif()

if (OPT_CHAFA)
    add_subdirectory(chafa)
endif()

if (OPT_MPRIS)
    add_subdirectory(mpris)
endif()

if (OPT_PIPEWIRE)
    add_subdirectory(pipewire)
endif()

if (OPT_SNDIO OR CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    add_subdirectory(sndio)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    add_subdirectory(apple)
endif()

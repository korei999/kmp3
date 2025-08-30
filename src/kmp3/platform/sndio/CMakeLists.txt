# src/kmp3/platform/sndio/CMakeLists.txt

pkg_check_modules(SNDIO sndio)
if (SNDIO_FOUND)
    allPackageFlags(TARGET ${kmp3} NAME SNDIO)

    target_compile_definitions(${kmp3} PRIVATE "-DOPT_SNDIO")
    target_sources(${kmp3} PRIVATE
        Mixer.cc
    )
endif()

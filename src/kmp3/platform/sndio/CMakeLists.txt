# src/kmp3/platform/sndio/CMakeLists.txt

pkg_check_modules(SNDIO sndio)
if (SNDIO_FOUND)
    set(bAtLeastOneAudioDriver true PARENT_SCOPE)
    allPackageFlags(TARGET ${subProj} NAME SNDIO)

    target_compile_definitions(${subProj} PRIVATE "-DOPT_SNDIO")
    target_sources(${subProj} PRIVATE
        Mixer.cc
    )
endif()

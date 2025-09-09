# src/kmp3/platform/alsa/CMakeLists.txt

pkg_check_modules(ALSA alsa)
if (ALSA_FOUND)
    set(bAtLeastOneAudioDriver true PARENT_SCOPE)
    allPackageFlags(TARGET ${subProj} NAME ALSA)

    target_compile_definitions(${subProj} PRIVATE "-DOPT_ALSA")
    target_sources(
        ${subProj} PRIVATE
        Mixer.cc
    )
endif()

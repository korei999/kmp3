# src/kmp3/platform/alsa/CMakeLists.txt

pkg_check_modules(ALSA alsa)
if (ALSA_FOUND)
    allPackageFlags(TARGET ${kmp3} NAME ALSA)

    target_compile_definitions(${kmp3} PRIVATE "-DOPT_ALSA")
    target_sources(
        ${kmp3} PRIVATE
        Mixer.cc
    )
endif()

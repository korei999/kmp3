# src/kmp3/platform/pipewire/CMakeLists.txt

pkg_check_modules(PIPEWIRE libpipewire-0.3)
if (PIPEWIRE_FOUND)
    allPackageFlags(TARGET ${kmp3} NAME PIPEWIRE)

    target_compile_definitions(${kmp3} PRIVATE "-DOPT_PIPEWIRE")
    target_sources(${kmp3} PRIVATE
        Mixer.cc
    )
endif()

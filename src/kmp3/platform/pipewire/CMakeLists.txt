# src/kmp3/platform/pipewire/CMakeLists.txt

pkg_check_modules(PIPEWIRE libpipewire-0.3)
if (PIPEWIRE_FOUND)
    allPackageFlags(TARGET ${subProj} NAME PIPEWIRE)

    target_compile_definitions(${subProj} PRIVATE "-DOPT_PIPEWIRE")
    target_sources(${subProj} PRIVATE
        Mixer.cc
    )
endif()

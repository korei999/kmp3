# src/kmp3/platform/apple/CMakeLists.txt

target_compile_definitions(${kmp3} PRIVATE "-DOPT_COREAUDIO")
target_link_libraries(${kmp3} PRIVATE
    "-framework CoreAudio"
    "-framework AudioToolbox"
    "-framework CoreServices"
)
target_sources(${kmp3} PRIVATE
    Mixer.cc
)

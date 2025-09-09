# src/kmp3/platform/coreaudio/CMakeLists.txt

target_compile_definitions(${subProj} PRIVATE "-DOPT_COREAUDIO")
target_link_libraries(${subProj} PRIVATE
    "-framework CoreAudio"
    "-framework AudioToolbox"
    "-framework CoreServices"
)
target_sources(${subProj} PRIVATE
    Mixer.cc
)

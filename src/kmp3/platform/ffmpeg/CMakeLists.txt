# src/kmp3/platform/ffmpeg/CMakeLists.txt

pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswresample)
# allPackageFlags(TARGET ${subProj} NAME FFMPEG)

target_include_directories(${subProj} PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_compile_options(${subProj} PRIVATE ${FFMPEG_CFLAGS})

target_sources(${subProj} PRIVATE
    Decoder.cc
    dll.cc
)

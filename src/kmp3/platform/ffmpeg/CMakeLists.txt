# src/kmp3/platform/ffmpeg/CMakeLists.txt

pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswresample)
allPackageFlags(TARGET ${kmp3} NAME FFMPEG)

target_sources(${kmp3} PRIVATE
    Decoder.cc
)

# src/kmp3/platform/ffmpeg/CMakeLists.txt

pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswresample)
allPackageFlags(TARGET ${subProj} NAME FFMPEG)

target_sources(${subProj} PRIVATE
    Decoder.cc
)

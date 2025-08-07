cmake_minimum_required(VERSION 3.20)

file(STRINGS name NAME)
message(STATUS "NAME: '${NAME}'")

project(${NAME} VERSION 0.1 LANGUAGES CXX C)
message(STATUS "CMAKE_PROJECT_VERSION: '${CMAKE_PROJECT_VERSION}'")

option(OPT_ALSA "" ON)
option(OPT_PIPEWIRE "" ON)
option(OPT_SNDIO "" ON)
option(OPT_CHAFA "" ON)
option(OPT_MPRIS "" ON)
option(OPT_MIMALLOC "" OFF)
option(OPT_SSE4_2 "" OFF)
option(OPT_AVX2 "" OFF)
option(OPT_MARCH_NATIVE "" OFF)
option(OPT_PRECOMPILE_ADT "" ON)
option(OPT_FTIME_TRACE "" OFF)

if (OPT_SSE4_2)
    add_definitions("-DADT_SSE4_2")

    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/arch:SSE4.2)
    else()
        add_compile_options(-msse4.2)
    endif()
endif()

if (OPT_AVX2)
    add_definitions("-DADT_AVX2")

    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/arch:AVX2)
    else()
        add_compile_options(-mavx2 -mfma)
    endif()
endif()

if (OPT_MARCH_NATIVE)
    add_compile_options(-march=native)
endif()

if (OPT_ANALYZE)
    add_compile_options(-fanalyzer)
endif()

if (OPT_MIMALLOC)
    include(FetchContent)
    FetchContent_Declare(
        MIMALLOC
        GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
        GIT_TAG dev3
    )
    FetchContent_MakeAvailable(MIMALLOC)
    FetchContent_GetProperties(MIMALLOC
        SOURCE_DIR MIMALLOC_SOURCE_DIR
        BINARY_DIR MIMALLOC_BINARY_DIR
    )
    message(STATUS "MIMALLOC_SOURCE_DIR: '${MIMALLOC_SOURCE_DIR}'")
    add_definitions("-DADT_USE_MIMALLOC")
endif()

if (OPT_FTIME_TRACE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-trace")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "CMAKE_C_COMPILER_ID: '${CMAKE_C_COMPILER_ID}'")
message(STATUS "CMAKE_CXX_COMPILER_ID: '${CMAKE_CXX_COMPILER_ID}'")
message(STATUS "CMAKE_CXX_STANDARD: '${CMAKE_CXX_STANDARD}'")

add_definitions("-DADT_DEFER_LESS_TYPING")
add_definitions("-DADT_LOGS_LESS_TYPING")

include_directories(BEFORE "src")
include_directories(BEFORE "include")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions("-D_DEFAULT_SOURCE")
    add_definitions("-D_XOPEN_SOURCE")
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_definitions("-D_CRT_SECURE_NO_WARNINGS")
endif()

if (CMAKE_BUILD_TYPE MATCHES "Release")
    # set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY WIN32_EXECUTABLE TRUE)

    # if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(-flto=auto)
    add_link_options(-flto=auto)
    # endif()
endif()

if (CMAKE_BUILD_TYPE MATCHES "Asan")
    set(CMAKE_BUILD_TYPE "Debug")
    add_definitions("-DMI_TRACK_ASAN=ON")

    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(-fsanitize=address)
    else()
        add_compile_options(-O0 -g3 -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=address)

        if (CMAKE_SYSTEM_NAME MATCHES "Windows")
            # doesn't work...
        else()
            add_link_options(-fsanitize=undefined -fsanitize=address)
        endif()
    endif()
endif()

message(STATUS "CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}'")

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/wd4146 /wd4200 /wd4312)
    add_compile_options(/utf-8)
else()
    add_compile_options(-Wall -Wextra -fms-extensions -Wno-missing-braces)
endif()

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions("-DADT_LOGS")
    add_definitions("-DADT_DBG_MEMORY")

    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/wd4267 /wd4101 /wd4200 /wd4244)
    else()
        add_compile_options(
            -Wno-unused-parameter
            -Wno-unused-variable
            -Wno-unused-function
            -Wno-unused-but-set-variable
            -Wno-missing-field-initializers
        )
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-class-memaccess")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wno-c99-designator)
endif()

message(STATUS "CMAKE_SYSTEM_NAME: '${CMAKE_SYSTEM_NAME}'")
message(STATUS "CMAKE_SOURCE_DIR: '${CMAKE_SOURCE_DIR}'")

add_executable(
    ${CMAKE_PROJECT_NAME}
    src/main.cc
    src/app.cc
    src/frame.cc
    src/Player.cc
    src/common.cc
    src/audio.cc

    src/platform/ffmpeg/Decoder.cc

    src/platform/ansi/Win.cc
    src/platform/ansi/WinDraw.cc
    src/platform/ansi/WinInput.cc
    src/platform/ansi/TextBuff.cc
)

if (OPT_PRECOMPILE_ADT)
    # file(GLOB_RECURSE ADT_PRECOMPILED_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/adt/*")

    set(ADT_PRECOMPILED_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/String.inc
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/String.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/Thread.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/Arena.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/Vec.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/atomic.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/QueueArray.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/print.inc
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/print.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/logs.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/Span2D.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/Span.inc
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/Span.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/ScratchBuffer.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/math.inc
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/math.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/Array.hh
        ${CMAKE_CURRENT_SOURCE_DIR}/include/adt/file.hh
    )

    if (NOT OPT_MIMALLOC)
        list(REMOVE_ITEM
            ADT_PRECOMPILED_HEADERS
            "${CMAKE_CURRENT_SOURCE_DIR}/include/adt/MiMalloc.hh"
        )
    endif()

    target_precompile_headers(
        ${CMAKE_PROJECT_NAME} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${ADT_PRECOMPILED_HEADERS}>"
    )
endif()

if (OPT_MIMALLOC)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE mimalloc-static)
endif()

find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswresample)
include_directories(BEFORE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARIES})

if (OPT_ALSA)
    pkg_check_modules(ALSA alsa)
    if (ALSA_FOUND)
        add_definitions("-DOPT_ALSA")
        include_directories(BEFORE ${ALSA_INCLUDE_DIRS})
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${ALSA_LIBRARIES})
        target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${ALSA_LIBRARY_DIRS})

        target_sources(
            ${CMAKE_PROJECT_NAME} PRIVATE
            "src/platform/alsa/Mixer.cc"
        )
    endif()
endif()

if (OPT_PIPEWIRE)
    pkg_check_modules(PIPEWIRE libpipewire-0.3)
    if (PIPEWIRE_FOUND)
        add_definitions("-DOPT_PIPEWIRE")
        include_directories(BEFORE ${PIPEWIRE_INCLUDE_DIRS})
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${PIPEWIRE_LIBRARIES})
        target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${PIPEWIRE_LIBRARY_DIRS})

        target_sources(
            ${CMAKE_PROJECT_NAME} PRIVATE
            "src/platform/pipewire/Mixer.cc"
        )
    endif()
endif()

if (OPT_SNDIO OR CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    pkg_check_modules(SNDIO sndio)
    if (SNDIO_FOUND)
        add_definitions("-DOPT_SNDIO")
        include_directories(BEFORE ${SNDIO_INCLUDE_DIRS})
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${SNDIO_LIBRARIES})
        target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${SNDIO_LIBRARY_DIRS})

        target_sources(
            ${CMAKE_PROJECT_NAME} PRIVATE
            "src/platform/sndio/Mixer.cc"
        )
    endif()
endif()

if (OPT_CHAFA)
    pkg_check_modules(CHAFA chafa)
    pkg_check_modules(SWSCALE libswscale)

    if (CHAFA_FOUND AND SWSCALE_FOUND)
        message(STATUS "CHAFA_VERSION: '${CHAFA_VERSION}'")
        if (${CHAFA_VERSION} VERSION_GREATER_EQUAL 1.14)
            add_definitions("-DOPT_CHAFA_SYMBOLS")
        endif()

        include_directories(BEFORE ${CHAFA_INCLUDE_DIRS} ${SWSCALE_INCLUDE_DIRS})
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${CHAFA_LIBRARIES} ${SWSCALE_LIBRARIES})
        target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CHAFA_LIBRARY_DIRS})
        add_definitions("-DOPT_CHAFA")
        target_sources(
            ${CMAKE_PROJECT_NAME} PRIVATE
            "src/platform/chafa/chafa.cc"
        )
    endif()
endif()

if(OPT_MPRIS)
    pkg_check_modules(SYSTEMD libsystemd)
    if (SYSTEMD_FOUND)
        include_directories(BEFORE ${SYSTEMD_INCLUDE_DIRS})
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${SYSTEMD_LIBRARIES})
        target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${SYSTEMD_LIBRARY_DIRS})
        target_sources(${CMAKE_PROJECT_NAME} PRIVATE "src/mpris.cc")
        add_definitions("-DOPT_MPRIS")
    else()
        pkg_check_modules(BASU basu)
        if (BASU_FOUND)
            include_directories(BEFORE ${BASU_INCLUDE_DIRS})
            target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${BASU_LIBRARIES})
            target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${BASU_LIBRARY_DIRS})
            target_sources(${CMAKE_PROJECT_NAME} PRIVATE "src/mpris.cc")
            add_definitions("-DOPT_MPRIS -DOPT_BASU")
        endif()
    endif()
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    add_definitions("-DOPT_COREAUDIO")
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreServices"
    )
    target_sources(
        ${CMAKE_PROJECT_NAME} PRIVATE
        "src/platform/apple/apple.cc"
    )
endif()

install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

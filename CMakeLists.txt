cmake_minimum_required(VERSION 3.20)

file(STRINGS name NAME)
message(STATUS "NAME: '${NAME}'")

project(${NAME} VERSION 0.1 LANGUAGES CXX C)
message(STATUS "CMAKE_PROJECT_VERSION: '${CMAKE_PROJECT_VERSION}'")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "CMAKE_C_COMPILER_ID: '${CMAKE_C_COMPILER_ID}'")
message(STATUS "CMAKE_CXX_COMPILER_ID: '${CMAKE_CXX_COMPILER_ID}'")
message(STATUS "CMAKE_CXX_STANDARD: '${CMAKE_CXX_STANDARD}'")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

include_directories(BEFORE "src")
include_directories(BEFORE "libs")

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions("-D_DEFAULT_SOURCE")
    add_definitions("-D_XOPEN_SOURCE")
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_definitions("-D_CRT_SECURE_NO_WARNINGS")
endif()

option(OPT_PIPEWIRE "" ON)
option(OPT_CHAFA "" ON)
option(OPT_MPRIS "" ON)
option(OPT_SIXEL "" OFF)
option(OPT_NCURSES "" OFF)
option(OPT_MARCH_NATIVE "" OFF)

if (OPT_MARCH_NATIVE)
    add_compile_options(-march=native)
endif()

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-trace")

add_definitions("-DADT_DEFER_LESS_TYPING")
add_definitions("-DADT_LOGS_LESS_TYPING")

if (CMAKE_BUILD_TYPE MATCHES "Release" AND CMAKE_SYSTEM_NAME MATCHES "Windows")
    set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY WIN32_EXECUTABLE TRUE)

    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(-flto)
        add_link_options(-flto)
    endif()
endif()

if (CMAKE_BUILD_TYPE MATCHES "Asan")
    set(CMAKE_BUILD_TYPE "Debug")

    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(-fsanitize=address)
    else()
        add_compile_options(-O0 -g3 -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=address)

        if (CMAKE_SYSTEM_NAME MATCHES "Windows")
            add_compile_options(-shared-libsan)
            # doesn't work...
            # target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE clang_rt.asan_cxx-x86_64.lib clang_rt.asan-x86_64.lib)
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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-class-memaccess -Wno-cast-function-type")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wno-c99-designator -Wno-cast-function-type-mismatch)
endif()

message(STATUS "CMAKE_SYSTEM_NAME: '${CMAKE_SYSTEM_NAME}'")
message(STATUS "CMAKE_SOURCE_DIR: '${CMAKE_SOURCE_DIR}'")

add_executable(
    ${CMAKE_PROJECT_NAME}
    src/main.cc
    src/app.cc
    src/frame.cc
    src/Player.cc
    src/ffmpeg.cc
    src/common.cc
    src/platform/termbox2/window.cc
    src/platform/termbox2/input.cc
    src/platform/ansi/Win.cc
    src/platform/ansi/WinDraw.cc
    src/platform/ansi/WinInput.cc
)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswresample)
    include_directories(BEFORE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(
        ${CMAKE_PROJECT_NAME} PRIVATE
        ${FFMPEG_LIBRARIES}
    )

    if (OPT_PIPEWIRE)
        pkg_check_modules(PIPEWIRE libpipewire-0.3)
        if (PIPEWIRE_FOUND)
            add_definitions("-DUSE_PIPEWIRE")
            include_directories(BEFORE ${PIPEWIRE_INCLUDE_DIRS})
            target_link_libraries(
                ${CMAKE_PROJECT_NAME} PRIVATE
                ${PIPEWIRE_LIBRARIES}
            )

            target_sources(${CMAKE_PROJECT_NAME} PRIVATE
                "src/platform/pipewire/Mixer.cc"
            )
        endif()
    endif()

    if (OPT_SIXEL)
        pkg_check_modules(SIXEL libsixel)
        if (SIXEL_FOUND)
            include_directories(BEFORE ${SIXEL_INCLUDE_DIRS})
            target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${SIXEL_LIBRARIES})
            add_definitions("-DUSE_SIXEL")

            target_sources(
                ${CMAKE_PROJECT_NAME} PRIVATE
                "src/platform/sixel/Img.cc"
            )
        endif()
    endif()

    if (OPT_NCURSES)
        pkg_check_modules(NCURSES ncursesw)
        if (NCURSES_FOUND)
            include_directories(BEFORE ${NCURSES_INCLUDE_DIRS})
            target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${NCURSES_LIBRARIES})
            add_definitions("-DUSE_NCURSES")
            add_definitions("-DNCURSES_WIDECHAR")
            target_sources(
                ${CMAKE_PROJECT_NAME} PRIVATE
                "src/platform/ncurses/Win.cc"
                "src/platform/ncurses/input.cc"
            )
        endif()
    endif()

    if (OPT_CHAFA)
        pkg_check_modules(CHAFA chafa)
        pkg_check_modules(SWSCALE libswscale)

        if (CHAFA_FOUND AND SWSCALE_FOUND)
            message(STATUS "CHAFA_VERSION: '${CHAFA_VERSION}'")
            if (${CHAFA_VERSION} VERSION_GREATER_EQUAL 1.14)
                add_definitions("-DUSE_CHAFA_SYMBOLS")
            endif()

            include_directories(BEFORE ${CHAFA_INCLUDE_DIRS} ${SWSCALE_INCLUDE_DIRS})
            target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${CHAFA_LIBRARIES} ${SWSCALE_LIBRARIES})
            add_definitions("-DUSE_CHAFA")
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
            target_sources(${CMAKE_PROJECT_NAME} PRIVATE "src/mpris.cc")
            add_definitions("-DUSE_MPRIS")
        else()
            pkg_check_modules(BASU basu)
            if (${BASU_FOUND})
                include_directories(BEFORE ${BASU_INCLUDE_DIRS})
                target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${BASU_LIBRARIES})
                target_sources(${CMAKE_PROJECT_NAME} PRIVATE "src/mpris.cc")
                add_definitions("-DUSE_MPRIS -DUSE_BASU")
            endif()
        endif()
    endif()
endif()

install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

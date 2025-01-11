cmake_minimum_required(VERSION 3.20)

file(STRINGS name NAME)
message(STATUS "NAME: '${NAME}'")

project(${NAME} VERSION 0.1 LANGUAGES CXX C)
message(STATUS "CMAKE_PROJECT_VERSION: '${CMAKE_PROJECT_VERSION}'")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS OFF)

message(STATUS "CMAKE_C_COMPILER_ID: '${CMAKE_C_COMPILER_ID}'")
message(STATUS "CMAKE_CXX_COMPILER_ID: '${CMAKE_CXX_COMPILER_ID}'")

set(CMAKE_CXX_STANDARD 20)

message(STATUS "CMAKE_CXX_STANDARD: '${CMAKE_CXX_STANDARD}'")

set(CMAKE_CXX_EXTENSIONS OFF)

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

add_executable(
    ${CMAKE_PROJECT_NAME}
    src/main.cc
    src/app.cc
    src/frame.cc
    src/Player.cc
    src/ffmpeg.cc
    src/mpris.cc
    src/common.cc
    src/platform/pipewire/Mixer.cc
    src/platform/termbox2/window.cc
    src/platform/termbox2/input.cc
    src/platform/ansi/Win.cc
    src/platform/ansi/WinDraw.cc
    src/platform/ansi/WinInput.cc
)

option(OPT_NCURSES "ncurses support" OFF)
option(OPT_MPRIS "mpris support" ON)
option(OPT_SIXEL "sixel support" OFF)
option(OPT_CHAFA "chafa support" ON)
option(OPT_MARCH_NATIVE OFF)

if (OPT_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
endif()

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-trace")

add_definitions("-DADT_DEFER_LESS_TYPING")
add_definitions("-DADT_LOGS_LESS_TYPING")

if (CMAKE_BUILD_TYPE MATCHES "Release" AND CMAKE_SYSTEM_NAME MATCHES "Windows")
    set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY WIN32_EXECUTABLE TRUE)

    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin")
    endif()
endif()

# debug logs defines

if (CMAKE_BUILD_TYPE MATCHES "Asan")
    set(CMAKE_BUILD_TYPE "Debug")

    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    else()

        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g3 -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=address")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g3 -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=address")

        if (CMAKE_SYSTEM_NAME MATCHES "Windows")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -shared-libsan")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -shared-libsan")
            # doesn't work...
            # target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE clang_rt.asan_cxx-x86_64.lib clang_rt.asan-x86_64.lib)
        else()
            add_link_options(-fsanitize=undefined -fsanitize=address)
        endif()
    endif()
endif()

message(STATUS "CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}'")

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /GR") # disable exceptions
    # add_definitions("/D_HAS_EXCEPTIONS=0")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4146 /wd4200 /wd4312")
    set(CMAKE_CXX_FLAGS "/utf-8")
    set(CMAKE_C_FLAGS "/utf-8")
else()
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics -fno-rtti -fno-exceptions -fno-unwind-tables")
    # if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    #     SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nodefaultlibs -lc -lm")
    # endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fms-extensions -Wno-missing-braces")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fms-extensions -Wno-missing-braces")
endif()

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions("-DADT_LOGS")
    add_compile_definitions("-DADT_DBG_MEMORY")

    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4267 /wd4101 /wd4200 /wd4244")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-field-initializers -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-cast-function-type-mismatch")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmissing-field-initializers -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-cast-function-type-mismatch")
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-class-memaccess")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c99-designator")
endif()

message(STATUS "CMAKE_SYSTEM_NAME: '${CMAKE_SYSTEM_NAME}'")
message(STATUS "CMAKE_SOURCE_DIR: '${CMAKE_SOURCE_DIR}'")

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LINUX_PKGS REQUIRED libpipewire-0.3 libavformat libavcodec libavutil libswresample)
    include_directories(BEFORE ${LINUX_PKGS_INCLUDE_DIRS})
    target_link_libraries(
        ${CMAKE_PROJECT_NAME} PRIVATE
        ${LINUX_PKGS_LIBRARIES}
    )

    if (OPT_SIXEL)
        pkg_check_modules(SIXEL libsixel)
        if (${SIXEL_FOUND})
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
        if (${NCURSES_FOUND})
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
        pkg_check_modules(CHAFA REQUIRED chafa libswscale)
        if (${CHAFA_FOUND})
            include_directories(BEFORE ${CHAFA_INCLUDE_DIRS})
            target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${CHAFA_LIBRARIES})
            add_definitions("-DUSE_CHAFA")
            target_sources(
                ${CMAKE_PROJECT_NAME} PRIVATE
                "src/platform/chafa/chafa.cc"
            )
        endif()
    endif()

    if(OPT_MPRIS)
        pkg_check_modules(SYSTEMD libsystemd)
        if (${SYSTEMD_FOUND})
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

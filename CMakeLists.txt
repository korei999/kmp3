cmake_minimum_required(VERSION 3.20)

file(STRINGS name projName)
message(STATUS "projName: '${projName}'")

project(${projName} VERSION 1.0 LANGUAGES CXX C)
message(STATUS "CMAKE_PROJECT_VERSION: '${CMAKE_PROJECT_VERSION}'")

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE PROJECT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if (NOT PROJECT_COMMIT_HASH)
    set(PROJECT_COMMIT_HASH "no-git")
endif()

add_definitions("-DPROJECT_COMMIT_HASH=\"${PROJECT_COMMIT_HASH}\"")
add_definitions("-DPROJECT_NAME=\"${projName}\"")
add_definitions("-DPROJECT_VERSION=\"${CMAKE_PROJECT_VERSION}\"")
add_definitions("-DPROJECT_VERSION_MAJOR=\"${CMAKE_PROJECT_VERSION_MAJOR}\"")
add_definitions("-DPROJECT_VERSION_MINOR=\"${CMAKE_PROJECT_VERSION_MINOR}\"")

option(OPT_ALSA "" ON)
option(OPT_PIPEWIRE "" ON)
option(OPT_SNDIO "" ON)
option(OPT_CHAFA "" ON)
option(OPT_MPRIS "" ON)
option(OPT_MIMALLOC "" OFF)
option(OPT_SSE4_2 "" OFF)
option(OPT_AVX2 "" OFF)
option(OPT_MARCH_NATIVE "" OFF)
option(OPT_ANALYZE "" OFF)
option(OPT_FTIME_TRACE "" OFF)
option(OPT_ADT_LOGGER_DISABLE "" ON)
option(OPT_STATIC_LIBCXX "" OFF)

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
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "CMAKE_C_COMPILER_ID: '${CMAKE_C_COMPILER_ID}'")
message(STATUS "CMAKE_CXX_COMPILER_ID: '${CMAKE_CXX_COMPILER_ID}'")
message(STATUS "CMAKE_CXX_STANDARD: '${CMAKE_CXX_STANDARD}'")

add_definitions("-DADT_DEFER_LESS_TYPING")
add_definitions("-DADT_ASSERT_USE_LOGGER")

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
    if (OPT_STATIC_LIBCXX)
        add_link_options(-static-libstdc++ -static-libgcc)
    endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # add_compile_options(-Wno-c99-designator)
endif()

message(STATUS "CMAKE_SYSTEM_NAME: '${CMAKE_SYSTEM_NAME}'")
message(STATUS "CMAKE_SOURCE_DIR: '${CMAKE_SOURCE_DIR}'")

find_package(PkgConfig REQUIRED)

function(allPackageFlags)
    set(oneValueArgs TARGET NAME)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "" ${ARGN})

    if (DEFINED ${ARG_NAME}_INCLUDE_DIRS)
        target_include_directories(${ARG_TARGET} PRIVATE ${${ARG_NAME}_INCLUDE_DIRS})
    endif()

    if (DEFINED ${ARG_NAME}_LIBRARY_DIRS)
        target_link_directories(${ARG_TARGET} PRIVATE ${${ARG_NAME}_LIBRARY_DIRS})
    endif()

    if (DEFINED ${ARG_NAME}_LIBRARIES)
        target_link_libraries(${ARG_TARGET} PRIVATE ${${ARG_NAME}_LIBRARIES})
    endif()

    if (DEFINED ${ARG_NAME}_CFLAGS)
        target_compile_options(${ARG_TARGET} PRIVATE ${${ARG_NAME}_CFLAGS})
    endif()
endfunction()

file(GLOB_RECURSE ADT_PRECOMPILED_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/adt/*")
if (NOT OPT_MIMALLOC)
    list(REMOVE_ITEM
        ADT_PRECOMPILED_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/adt/MiMalloc.hh"
    )
endif()

add_library(ADTLIB_PCH INTERFACE)
target_precompile_headers(ADTLIB_PCH INTERFACE
    "$<$<COMPILE_LANGUAGE:CXX>:${ADT_PRECOMPILED_HEADERS}>"
)

include_directories("include")

add_executable(${projName})

if (CMAKE_BUILD_TYPE MATCHES "Release")
    target_link_options(${projName} PRIVATE -s)
    if (OPT_ADT_LOGGER_DISABLE)
        add_definitions("-DADT_LOGGER_DISABLE")
    endif()
endif()

add_subdirectory(src/kmp3)

install(TARGETS ${projName} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

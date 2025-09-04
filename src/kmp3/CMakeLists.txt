# src/kmp3/CMakeLists.txt

set(subProj ${projName})
target_sources(${subProj} PRIVATE
    app.cc
    audio.cc
    common.cc
    frame.cc
    main.cc
    Player.cc
)
target_include_directories(${subProj} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(${subProj} PRIVATE ADTLIB_PCH)

if (OPT_MIMALLOC)
    target_link_libraries(${subProj} PRIVATE mimalloc-static)
endif()

add_subdirectory(platform)

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

add_subdirectory(platform)

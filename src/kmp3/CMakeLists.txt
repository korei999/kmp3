# src/kmp3/CMakeLists.txt

target_sources(${kmp3} PRIVATE
    app.cc
    audio.cc
    common.cc
    frame.cc
    main.cc
    Player.cc
)

add_subdirectory(platform)

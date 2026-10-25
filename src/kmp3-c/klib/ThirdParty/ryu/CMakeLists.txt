add_library(ryu-static STATIC
    d2fixed.c
    d2s.c
    f2s.c
    s2d.c
    s2f.c
)
target_include_directories(ryu-static PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

add_subdirectory(ThirdParty)

add_library(klib-static STATIC
    print.c
    StringView.c
    String.c
    Arena.c
    IAllocator.c
    RingBuffer.c
    ThreadPool.c
    Logger.c
    Ctx.c
    file.c
    time.c
    CmdLine.c
    assert.c
    RingMPSC.c
    Json.c
)

target_link_libraries(klib-static PRIVATE ryu-static)

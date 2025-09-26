set(ADT_PRECOMPILED_HEADERS
    Arena.hh
    # ArenaList.hh
    ArgvParser.hh
    Array.hh
    assert.hh
    atomic.hh
    # bin.hh
    # BufferAllocator.hh
    defer.hh
    # Directory.hh
    enum.hh
    Exception.hh
    file.hh
    # FuncBuffer.hh
    # hash.hh
    # Heap.hh
    # IAllocator.hh
    # ILogger.hh
    # IThreadPool.hh
    # List.hh
    Logger.hh
    # Map.hh
    math.hh
    # MiMalloc.hh
    Opt.hh
    # Pair.hh
    # Pipeline.hh
    # PoolAllocator.hh
    # Pool.hh
    # PoolSOA.hh
    print.hh
    QueueArray.hh
    # Queue.hh
    # RBTree.hh
    # RefCount.hh
    # ReverseIt.hh
    # rng.hh
    # Set.hh
    # simd.hh
    # SList.hh
    # SOA.hh
    # sort.hh
    Span2D.hh
    # Span.hh
    # SplayTree.hh
    StdAllocator.hh
    # String.hh
    # Thread.hh
    ThreadPool.hh
    Timer.hh
    # types.hh
    # utf8.hh
    # utils.hh
    Vec.hh
    # VecSOA.hh
    # View.hh
    # wcwidth.hh
)
list(TRANSFORM ADT_PRECOMPILED_HEADERS PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/include/adt/")

if (NOT OPT_MIMALLOC)
    list(REMOVE_ITEM
        ADT_PRECOMPILED_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/adt/MiMalloc.hh"
    )
endif()

#pragma once

#include "QueueArray.hh"
#include "ScratchBuffer.hh"
#include "Thread.hh"
#include "atomic.hh"
#include "defer.hh"
#include "StdAllocator.hh"

namespace adt
{

struct IThreadPool
{
    struct Task
    {
        ThreadFn pfn {};
        void* pArg {};
    };

    /* */

    virtual void wait() = 0;

    virtual bool add(Task task) = 0;

    bool add(ThreadFn pfn, void* pArg) { return add({pfn, pArg}); }

    template<typename LAMBDA>
    bool
    addLambda(LAMBDA& t)
    {
        return add(+[](void* pArg) -> THREAD_STATUS
            {
                static_cast<LAMBDA*>(pArg)->operator()();
                return 0;
            },
            (void*)(&t)
        );
    }

    template<typename LAMBDA> requires(std::is_rvalue_reference_v<LAMBDA&&>)
    [[deprecated("rvalue lambdas cause use after free")]] bool addLambda(LAMBDA&& t) = delete;

    void addRetry(Task task) { while (!add(task)); }

    void addRetry(ThreadFn pfn, void* pArg) { while (!add(pfn, pArg)); }

    template<typename LAMBDA> requires(std::is_rvalue_reference_v<LAMBDA&&>)
    [[deprecated("rvalue lambdas cause use after free")]] void addLambdaRetry(LAMBDA&& t) = delete;
};

struct IThreadPoolWithMemory : IThreadPool
{
    virtual ScratchBuffer& scratchBuffer() = 0;
};

template<isize QUEUE_SIZE>
struct ThreadPool : IThreadPool
{
    Span<Thread> m_spThreads {};
    Mutex m_mtxQ {};
    CndVar m_cndQ {};
    CndVar m_cndWait {};
    void (*m_pfnLoopStart)(void*) {};
    void* m_pLoopStartArg {};
    void (*m_pfnLoopEnd)(void*) {};
    void* m_pLoopEndArg {};
    atomic::Int m_atomNActiveTasks {};
    atomic::Int m_atomBDone {};
    atomic::Int m_atomIdCounter {};
    bool m_bStarted {};
    QueueArray<Task, QUEUE_SIZE> m_qTasks {};

    /* */

    static inline thread_local int gtl_threadId {};

    /* */

    ThreadPool() = default;

    ThreadPool(IAllocator* pAlloc, int nThreads = utils::max(ADT_GET_NPROCS() - 1, 2));

    ThreadPool(
        IAllocator* pAlloc,
        void (*pfnOnLoopStart)(void*),
        void* pLoopStartArg,
        void (*pfnOnLoopEnd)(void*),
        void* pLoopEndArg,
        int nThreads = ADT_GET_NPROCS()
    );

    /* */

    virtual void wait() override;

    void destroy(IAllocator* pAlloc);

    virtual bool add(Task task) override;

protected:
    void start();
    THREAD_STATUS loop();
};

template<isize QUEUE_SIZE>
inline
ThreadPool<QUEUE_SIZE>::ThreadPool(IAllocator* pAlloc, int nThreads)
    : m_spThreads(pAlloc->zallocV<Thread>(nThreads), nThreads),
      m_mtxQ(Mutex::TYPE::PLAIN),
      m_cndQ(INIT),
      m_cndWait(INIT),
      m_atomBDone(false)
{
    start();
}

template<isize QUEUE_SIZE>
inline
ThreadPool<QUEUE_SIZE>::ThreadPool(
    IAllocator* pAlloc,
    void (*pfnOnLoopStart)(void*), void* pLoopStartArg,
    void (*pfnOnLoopEnd)(void*), void* pLoopEndArg,
    int nThreads
)
    : m_spThreads(pAlloc->zallocV<Thread>(nThreads), nThreads),
      m_mtxQ(Mutex::TYPE::PLAIN),
      m_cndQ(INIT),
      m_cndWait(INIT),
      m_pfnLoopStart(pfnOnLoopStart),
      m_pLoopStartArg(pLoopStartArg),
      m_pfnLoopEnd(pfnOnLoopEnd),
      m_pLoopEndArg(pLoopEndArg),
      m_atomBDone(false)
{
    start();
}

template<isize QUEUE_SIZE>
inline THREAD_STATUS
ThreadPool<QUEUE_SIZE>::loop()
{
    if (m_pfnLoopStart) m_pfnLoopStart(m_pLoopStartArg);
    ADT_DEFER( if (m_pfnLoopEnd) m_pfnLoopEnd(m_pLoopEndArg) );

    gtl_threadId = 1 + m_atomIdCounter.fetchAdd(1, atomic::ORDER::RELAXED);

    while (true)
    {
        Task task {};

        {
            LockGuard qLock {&m_mtxQ};

            while (m_qTasks.empty() && !m_atomBDone.load(atomic::ORDER::ACQUIRE))
                m_cndQ.wait(&m_mtxQ);

            if (m_atomBDone.load(atomic::ORDER::ACQUIRE))
                return 0;

            task = m_qTasks.popFront();
            m_atomNActiveTasks.fetchAdd(1, atomic::ORDER::SEQ_CST);
        }

        ADT_ASSERT(task.pfn, "pfn: '{}'", task.pfn);
        task.pfn(task.pArg);
        m_atomNActiveTasks.fetchSub(1, atomic::ORDER::SEQ_CST);

        {
            LockGuard qLock {&m_mtxQ};

            if (m_qTasks.empty() && m_atomNActiveTasks.load(atomic::ORDER::ACQUIRE) == 0)
                m_cndWait.signal();
        }
    }

    return THREAD_STATUS(0);
}

template<isize QUEUE_SIZE>
inline void
ThreadPool<QUEUE_SIZE>::start()
{
    for (auto& thread : m_spThreads)
    {
        thread = Thread(
            reinterpret_cast<ThreadFn>(methodPointerNonVirtual(&ThreadPool::loop)),
            this
        );
    }

    m_bStarted = true;

#ifndef NDEBUG
    print::err("ThreadPool: new pool with {} threads\n", m_spThreads.size());
#endif
}

template<isize QUEUE_SIZE>
inline void
ThreadPool<QUEUE_SIZE>::wait()
{
    LockGuard qLock {&m_mtxQ};

    while (!m_qTasks.empty() || m_atomNActiveTasks.load(atomic::ORDER::RELAXED) != 0)
        m_cndWait.wait(&m_mtxQ);
}

template<isize QUEUE_SIZE>
inline void
ThreadPool<QUEUE_SIZE>::destroy(IAllocator* pAlloc)
{
    wait();

    {
        LockGuard qLock {&m_mtxQ};
        m_atomBDone.store(true, atomic::ORDER::RELEASE);
    }

    m_cndQ.broadcast();

    for (auto& thread : m_spThreads)
        thread.join();

    {
        pAlloc->free(m_spThreads.data());
        m_mtxQ.destroy();
        m_cndQ.destroy();
        m_cndWait.destroy();
    }
}

template<isize QUEUE_SIZE>
inline bool
ThreadPool<QUEUE_SIZE>::add(Task task)
{
    ADT_ASSERT(m_bStarted, "forgot to `start()` this ThreadPool: (m_bStarted: '{}')", m_bStarted);

    isize i;

    {
        LockGuard lock {&m_mtxQ};
        i = m_qTasks.pushBack(task);
    }
    if (i != -1)
    {
        m_cndQ.signal();
        return true;
    }

    return false;
}

/* ThreadPool with ScratchBuffers created for each thread.
 * Any thread can access its own thread local buffer with `threadPool.scratch()`. */
template<isize QUEUE_SIZE>
struct ThreadPoolWithMemory : ThreadPool<QUEUE_SIZE>, IThreadPoolWithMemory
{
    using Base = ThreadPool<QUEUE_SIZE>;

    static inline thread_local u8* gtl_pScratchMem;
    static inline thread_local ScratchBuffer gtl_scratchBuff;

    /* */

    using ThreadPool<QUEUE_SIZE>::ThreadPool;

    ThreadPoolWithMemory(IAllocator* pAlloc, isize nBytesEachBuffer, int nThreads = utils::max(ADT_GET_NPROCS() - 1, 2))
        : Base(
            pAlloc,
            +[](void* p) { allocScratchBufferForThisThread(reinterpret_cast<isize>(p)); },
            reinterpret_cast<void*>(nBytesEachBuffer),
            +[](void*) { destroyScratchBufferForThisThread(); },
            nullptr,
            nThreads
        )
    {
        allocScratchBufferForThisThread(nBytesEachBuffer);
    }

    /* */

    virtual void wait() override { Base::wait(); }
    virtual bool add(Task task) override { return Base::add(task); }
    virtual ScratchBuffer& scratchBuffer() override { return gtl_scratchBuff; }

    template<typename LAMBDA>
    bool
    addLambda(LAMBDA& t)
    {
        return Base::addLambda(t);
    }

    void addRetry(Task task) { while (!Base::add(task)); }
    void addRetry(ThreadFn pfn, void* pArg) { addRetry({pfn, pArg}); }

    void
    destroy(IAllocator* pAlloc)
    {
        Base::destroy(pAlloc);
        destroyScratchBufferForThisThread();
    }

    /* `destroyScratchBufferForThisThread()` later. */
    void
    destroyKeepScratchBuffer(IAllocator* pAlloc)
    {
        Base::destroy(pAlloc);
    }

    /* */

    static void
    allocScratchBufferForThisThread(isize size)
    {
        ADT_ASSERT(gtl_pScratchMem == nullptr, "already allocated");

        gtl_pScratchMem = StdAllocator::inst()->zallocV<u8>(size);
        gtl_scratchBuff = ScratchBuffer {gtl_pScratchMem, size};
    }

    static void
    destroyScratchBufferForThisThread()
    {
        StdAllocator::inst()->free(gtl_pScratchMem);
        gtl_pScratchMem = {};
        gtl_scratchBuff = {};
    }
};

} /* namespace adt */

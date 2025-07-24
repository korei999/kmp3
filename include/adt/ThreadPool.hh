#pragma once

#include "QueueMPMC.hh"
#include "ScratchBuffer.hh"
#include "Thread.hh"
#include "Vec.hh"
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

    virtual const atomic::Int& nActiveTasks() = 0;

    virtual void wait() = 0;

    virtual bool add(Task task) = 0;

    virtual int nThreads() const noexcept = 0;

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

    template<typename LAMBDA>
    void
    addLambdaRetry(LAMBDA& t)
    {
        addRetry(+[](void* pArg) -> THREAD_STATUS
            {
                static_cast<LAMBDA*>(pArg)->operator()();
                return 0;
            },
            (void*)(&t)
        );
    }

    template<typename LAMBDA>
    bool
    addLambdaRetry(LAMBDA& t, int nRetries)
    {
        return addRetry(+[](void* pArg) -> THREAD_STATUS
            {
                static_cast<LAMBDA*>(pArg)->operator()();
                return 0;
            },
            (void*)(&t),
            nRetries
        );
    }

    template<typename LAMBDA> requires(std::is_rvalue_reference_v<LAMBDA&&>)
    [[deprecated("rvalue lambdas cause use after free")]] bool addLambda(LAMBDA&& t) = delete;

    void addRetry(Task task) { while (!add(task)); }

    bool
    addRetry(Task task, const int nRetries)
    {
        int nTried = 0;
        while (++nTried <= nRetries)
            if (add(task)) return true;
        return false;
    }

    void addRetry(ThreadFn pfn, void* pArg) { while (!add(pfn, pArg)); }

    bool addRetry(ThreadFn pfn, void* pArg, const int nRetries) { return addRetry({pfn, pArg}, nRetries); }

    void
    addRetryOrDo(ThreadFn pfn, void* pArg)
    {
        if (nActiveTasks().load(atomic::ORDER::ACQUIRE) >= nThreads()) pfn(pArg);
        else addRetry(pfn, pArg);
    }

    template<typename LAMBDA>
    void
    addLambdaRetryOrDo(LAMBDA& cl)
    {
        if (nActiveTasks().load(atomic::ORDER::ACQUIRE) >= nThreads()) cl();
        else addLambdaRetry(cl);
    }

    template<typename LAMBDA> requires(std::is_rvalue_reference_v<LAMBDA&&>)
    [[deprecated("rvalue lambdas cause use after free")]] void addLambdaRetryOrDo(LAMBDA&& cl) = delete;

    template<typename LAMBDA> requires(std::is_rvalue_reference_v<LAMBDA&&>)
    [[deprecated("rvalue lambdas cause use after free")]] void addLambdaRetry(LAMBDA&& t) = delete;
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
    atomic::Int m_atomBPollMode {};
    bool m_bStarted {};
    QueueMPMC<Task, QUEUE_SIZE> m_qTasks {};

    /* */

    static inline thread_local int gtl_threadId {};

    /* */

    ThreadPool() = default;

    ThreadPool(IAllocator* pAlloc, int nThreads = utils::max(ADT_GET_NPROCS() - 1, 1));

    ThreadPool(
        IAllocator* pAlloc,
        void (*pfnOnLoopStart)(void*),
        void* pLoopStartArg,
        void (*pfnOnLoopEnd)(void*),
        void* pLoopEndArg,
        int nThreads = ADT_GET_NPROCS()
    );

    /* */

    virtual const atomic::Int& nActiveTasks() override { return m_atomNActiveTasks; }

    virtual void wait() override;

    void destroy(IAllocator* pAlloc);

    virtual bool add(Task task) override;

    virtual int nThreads() const noexcept override { return m_spThreads.size(); }

    /* */

    void enablePollMode() noexcept;
    void disablePollMode() noexcept;

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
      m_atomBDone(false),
      m_qTasks(INIT)
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
      m_atomBDone(false),
      m_qTasks(INIT)
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

            while (m_atomBPollMode.load(atomic::ORDER::ACQUIRE) == false &&
                m_qTasks.empty() &&
                !m_atomBDone.load(atomic::ORDER::ACQUIRE)
            )
            {
                m_cndQ.wait(&m_mtxQ);
            }

            if (m_atomBDone.load(atomic::ORDER::ACQUIRE))
                return 0;

            task = m_qTasks.pop().valueOr({});
            if (task.pfn) m_atomNActiveTasks.fetchAdd(1, atomic::ORDER::SEQ_CST);
        }

        if (task.pfn)
        {
            task.pfn(task.pArg);
            m_atomNActiveTasks.fetchSub(1, atomic::ORDER::SEQ_CST);
        }

        {
            LockGuard qLock {&m_mtxQ};

            if (m_qTasks.empty() && m_atomNActiveTasks.load(atomic::ORDER::ACQUIRE) <= 0)
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
    print::err("[ThreadPool]: new pool with {} threads\n", m_spThreads.size());
#endif
}

template<isize QUEUE_SIZE>
inline void
ThreadPool<QUEUE_SIZE>::wait()
{
    LockGuard qLock {&m_mtxQ};

    while (!m_qTasks.empty() || m_atomNActiveTasks.load(atomic::ORDER::RELAXED) > 0)
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

    pAlloc->free(m_spThreads.data());
    m_mtxQ.destroy();
    m_cndQ.destroy();
    m_cndWait.destroy();
}

template<isize QUEUE_SIZE>
inline bool
ThreadPool<QUEUE_SIZE>::add(Task task)
{
    ADT_ASSERT(m_bStarted, "forgot to `start()` this ThreadPool: (m_bStarted: '{}')", m_bStarted);

    if (m_qTasks.push(task))
    {
        m_cndQ.signal();
        return true;
    }

    return false;
}

template<isize QUEUE_SIZE>
inline void
ThreadPool<QUEUE_SIZE>::enablePollMode() noexcept
{
    m_atomBPollMode.store(true, atomic::ORDER::RELEASE);
}

template<isize QUEUE_SIZE>
inline void
ThreadPool<QUEUE_SIZE>::disablePollMode() noexcept
{
    m_atomBPollMode.store(false, atomic::ORDER::RELEASE);
}

/* ThreadPool with ScratchBuffers created for each thread.
 * Any thread can access its own thread local buffer with `threadPool.scratch()`. */
template<isize QUEUE_SIZE>
struct ThreadPoolWithMemory : ThreadPool<QUEUE_SIZE>
{
    using Base = ThreadPool<QUEUE_SIZE>;
    using Task = Base::Task;

    /* */

    static inline thread_local u8* gtl_pScratchMem;
    static inline thread_local ScratchBuffer gtl_scratchBuff;

    /* */

    ThreadPoolWithMemory() = default;

    ThreadPoolWithMemory(IAllocator* pAlloc, isize nBytesEachBuffer, int nThreads = utils::max(ADT_GET_NPROCS() - 1, 1))
        : Base(
            pAlloc,
            +[](void* p) { allocScratchBufferForThisThread(reinterpret_cast<isize>(p)); },
            reinterpret_cast<void*>(nBytesEachBuffer),
            +[](void*) { destroyScratchBufferForThisThread(); },
            nullptr,
            nThreads
        )
    {
        ADT_ASSERT(nThreads > 0, "nThreads: {}", nThreads);
        allocScratchBufferForThisThread(nBytesEachBuffer);
    }

    /* */

    ScratchBuffer& scratchBuffer() { return gtl_scratchBuff; }

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

/* Usage example:
 * Vec<Future<Span<f32>>*> vFutures = parallelFor(&arena, &tp, Span<f32> {v},
 *     [](Span<f32> spBatch, isize offFrom0)
 *     {
 *         for (auto& e : spBatch) r += 1 + offFrom0;
 *     }
 * );
 * 
 *  for (auto* pF : vFutures) pF->wait(); */
template<typename THREAD_POOL_T, typename T, typename CL_PROC_BATCH>
[[nodiscard]] inline Vec<Future<Span<T>>*>
parallelFor(IArena* pArena, THREAD_POOL_T* pTp, Span<T> spData, CL_PROC_BATCH clProcBatch, isize minBatchSize = 1)
{
    if (spData.size() < 0) return {};

    const isize nThreads = pTp->nThreads();
    const isize batchSize = [&]
    {
        const isize len = spData.size() / nThreads;
        const isize div = len > 0 ? len : spData.size();
        return div < minBatchSize ? minBatchSize : div;
    }();
    const isize tailSize = spData.size() - nThreads*batchSize;

    struct Arg
    {
        Future<Span<T>> future {};
        Span<T> sp {};
        isize off {};
        decltype(clProcBatch) cl {};
    };

    Vec<Future<Span<T>>*> vFutures {pArena, tailSize > 0 ? nThreads + 1 : nThreads};

    auto clBatch = [&](isize off, isize size) {
        Arg* pArg = pArena->alloc<Arg>(INIT, Span<T> {spData.data() + off, size}, off, clProcBatch);
        pArg->future.data() = pArg->sp;

        vFutures.push(pArena, &pArg->future);

        pTp->addRetry(
            +[](void* p) -> THREAD_STATUS
            {
                Arg& arg = *static_cast<Arg*>(p);
                arg.cl(arg.sp, arg.off);
                arg.future.signal();
                return THREAD_STATUS(0);
            },
            pArg
        );
    };

    isize i = 0;
    for (; i < batchSize*nThreads; i += batchSize) clBatch(i, batchSize);
    if (tailSize > 0) clBatch(i, tailSize);

    return vFutures;
}

} /* namespace adt */

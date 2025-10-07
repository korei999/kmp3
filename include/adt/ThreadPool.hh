#pragma once

#include "IThreadPool.hh"

#include "FuncBuffer.hh"
#include "Gpa.hh"
#include "IArena.hh"
#include "Queue.hh"
#include "Thread.hh"
#include "Vec.hh"
#include "atomic.hh"
#include "defer.hh"

namespace adt
{

inline int
IThreadPool::optimalThreadCount() noexcept
{
    static const int s_count = utils::max(ADT_GET_NPROCS() - 1, 1);
    return s_count;
}

template<typename T>
inline
IThreadPool::Future<T>::Future(IThreadPool* pPool) noexcept
    : Base{INIT}, m_pPool{pPool}
{
    ADT_ASSERT(pPool != nullptr, "");
}

template<typename T>
inline void
IThreadPool::Future<T>::wait() noexcept
{
    Task task {UNINIT};
    while ((task = m_pPool->tryStealTask()))
        task();

    Base::wait();
}

template<typename T>
inline decltype(auto)
IThreadPool::Future<T>::waitData() noexcept
{
    Task task {};
    while ((task = m_pPool->tryStealTask()))
        task();

    if constexpr (std::is_same_v<T, void>) Base::wait();
    else return Base::waitData();
}

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
    QueueM<Task> m_qTasks {};
    isize m_arenaReserved {};
    ThreadLocalArena* (*m_pfnAllocArena)(isize reserve) {};


    /* */

    static inline thread_local int gtl_threadId {};
    static inline thread_local ThreadLocalArena* gtl_pArena {};

    /* */

    /* NOTE: ARENA_T&& use default constructor to match templalate (like `Arena{}`). */

    template<typename ARENA_T>
    ThreadPool(ARENA_T&& /* empty */, isize arenaReserve);

    template<typename ARENA_T>
    ThreadPool(ARENA_T&& /* empty */, isize qSize, isize arenaReserve, int nThreads = optimalThreadCount());

    template<typename ARENA_T>
    ThreadPool(
        ARENA_T&& /* empty */,
        void (*pfnOnLoopStart)(void*),
        void* pLoopStartArg,
        void (*pfnOnLoopEnd)(void*),
        void* pLoopEndArg,
        isize qSize,
        isize arenaReserve,
        int nThreads = optimalThreadCount()
    );

    /* */

    virtual const atomic::Int& nActiveTasks() const noexcept override { return m_atomNActiveTasks; }
    virtual void wait(bool bHelp) noexcept override;
    virtual bool addTask(void (*pfn)(void*), void* pArg, isize argSize) noexcept override;
    virtual int nThreads() const noexcept override { return m_spThreads.size(); }
    virtual Task tryStealTask() noexcept override;
    virtual usize threadId() noexcept override;
    virtual ThreadLocalArena* createArenaForThisThread(isize reserve) noexcept override;
    virtual void destroyArenaForThisThread() noexcept override;
    virtual ThreadLocalArena* arena() noexcept override;

    /* */

    void destroy() noexcept;

protected:
    void start();
    THREAD_STATUS loop();
};

template<typename ARENA_T>
inline
ThreadPool::ThreadPool(ARENA_T&&, isize arenaReserve)
    : m_arenaReserved{arenaReserve},
      m_pfnAllocArena{[](isize reserve) { return static_cast<ThreadLocalArena*>(Gpa::inst()->alloc<ARENA_T>(reserve)); }}
{
    start();
}

template <typename ARENA_T>
inline ThreadPool::ThreadPool(ARENA_T&&, isize qSize, isize arenaReserve, int nThreads)
    : m_spThreads(Gpa::inst()->zallocV<Thread>(nThreads), nThreads),
      m_mtxQ(Mutex::TYPE::PLAIN),
      m_cndQ(INIT),
      m_cndWait(INIT),
      m_qTasks(qSize),
      m_arenaReserved(arenaReserve),
      m_pfnAllocArena([](isize reserve) { return static_cast<ThreadLocalArena*>(Gpa::inst()->alloc<ARENA_T>(reserve)); })
{
    start();
}

template<typename ARENA_T>
inline
ThreadPool::ThreadPool(
    ARENA_T&&,
    void (*pfnOnLoopStart)(void*), void* pLoopStartArg,
    void (*pfnOnLoopEnd)(void*), void* pLoopEndArg,
    isize qSize,
    isize arenaReserve,
    int nThreads
)
    : m_spThreads(Gpa::inst()->zallocV<Thread>(nThreads), nThreads),
      m_mtxQ(Mutex::TYPE::PLAIN),
      m_cndQ(INIT),
      m_cndWait(INIT),
      m_pfnLoopStart(pfnOnLoopStart),
      m_pLoopStartArg(pLoopStartArg),
      m_pfnLoopEnd(pfnOnLoopEnd),
      m_pLoopEndArg(pLoopEndArg),
      m_qTasks(qSize),
      m_arenaReserved(arenaReserve),
      m_pfnAllocArena([](isize reserve) { return static_cast<ThreadLocalArena*>(Gpa::inst()->alloc<ARENA_T>(reserve)); })
{
    start();
}

inline THREAD_STATUS
ThreadPool::loop()
{
    if (m_pfnLoopStart) m_pfnLoopStart(m_pLoopStartArg);
    ADT_DEFER( if (m_pfnLoopEnd) m_pfnLoopEnd(m_pLoopEndArg) );

    ADT_ASSERT(m_pfnAllocArena != nullptr, "");

    gtl_pArena = m_pfnAllocArena(m_arenaReserved);
    ADT_DEFER(
        gtl_pArena->freeAll();
        gtl_pArena = nullptr;
    );

    gtl_threadId = m_atomIdCounter.fetchAdd(1, atomic::ORDER::RELAXED);

    while (true)
    {
        Task task {};

        {
            LockScope qLock {&m_mtxQ};

            while (m_qTasks.empty() && !m_atomBDone.load(atomic::ORDER::ACQUIRE))
                m_cndQ.wait(&m_mtxQ);

            if (m_atomBDone.load(atomic::ORDER::ACQUIRE))
                return 0;

            m_atomNActiveTasks.fetchAdd(1, atomic::ORDER::RELAXED);
            task = m_qTasks.popFront();
        }

        if (task) task();
        m_atomNActiveTasks.fetchSub(1, atomic::ORDER::RELEASE);

        {
            LockScope qLock {&m_mtxQ};

            if (m_qTasks.empty() && m_atomNActiveTasks.load(atomic::ORDER::ACQUIRE) <= 0)
                m_cndWait.signal();
        }
    }

    return THREAD_STATUS(0);
}

inline void
ThreadPool::start()
{
    m_atomIdCounter.fetchAdd(1, atomic::ORDER::RELAXED); /* Id 0 for the main thread. */
    for (auto& thread : m_spThreads)
    {
        thread = Thread(
            reinterpret_cast<ThreadFn>(methodPointerNonVirtual(&ThreadPool::loop)),
            this
        );
    }

    ADT_ASSERT(m_pfnAllocArena != nullptr, "");
    gtl_pArena = m_pfnAllocArena(m_arenaReserved);

    m_bStarted = true;

    while (m_atomIdCounter.load(atomic::ORDER::RELAXED) <= m_spThreads.size())
        Thread::yield();
}

inline void
ThreadPool::wait(bool bHelp) noexcept
{
    if (m_spThreads.size() <= 0) return;

    if (bHelp)
    {
again:
        m_mtxQ.lock();
        if (!m_qTasks.empty())
        {
            Task task = m_qTasks.popFront();
            m_mtxQ.unlock();
            if (task) task();

            goto again;
        }
        else
        {
            m_mtxQ.unlock();
        }
    }

    LockScope qLock {&m_mtxQ};
    while (!m_qTasks.empty() || m_atomNActiveTasks.load(atomic::ORDER::RELAXED) > 0)
        m_cndWait.wait(&m_mtxQ);
}

inline void
ThreadPool::destroy() noexcept
{
    if (!m_spThreads.empty())
    {
        wait(true);

        {
            LockScope qLock {&m_mtxQ};
            m_atomBDone.store(true, atomic::ORDER::RELEASE);
        }

        m_cndQ.broadcast();

        for (auto& thread : m_spThreads)
            thread.join();

        ADT_ASSERT(m_atomNActiveTasks.load(atomic::ORDER::ACQUIRE) == 0, "{}", m_atomNActiveTasks.load(atomic::ORDER::RELAXED));

        Gpa::inst()->free(m_spThreads.data());
        m_qTasks.destroy();
        m_mtxQ.destroy();
        m_cndQ.destroy();
        m_cndWait.destroy();
    }

    gtl_pArena->freeAll();
}

inline bool
ThreadPool::addTask(void (*pfn)(void*), void* pArg, isize argSize) noexcept
{
    ADT_ASSERT(m_bStarted, "forgot to `start()` this ThreadPool: (m_bStarted: '{}')", m_bStarted);

    isize i;
    {
        LockScope lock {&m_mtxQ};
        i = m_qTasks.emplaceBackNoGrow(pfn, pArg, argSize);
    }

    if (i != -1)
    {
        m_cndQ.signal();
        return true;
    }

    return false;
}

inline IThreadPool::Task
ThreadPool::tryStealTask() noexcept
{
    Task task {};

    {
        LockScope lock {&m_mtxQ};
        if (!m_qTasks.empty()) task = m_qTasks.popFront();
    }

    return task;
}

inline usize
ThreadPool::threadId() noexcept
{
    return gtl_threadId;
}

inline ThreadPool::ThreadLocalArena*
ThreadPool::createArenaForThisThread(isize reserve) noexcept
{
    ADT_ASSERT(gtl_pArena == nullptr, "arena already exists");
    return gtl_pArena = m_pfnAllocArena(reserve);
}

inline void
ThreadPool::destroyArenaForThisThread() noexcept
{
    ADT_ASSERT(gtl_pArena != nullptr, "createArenaForThisThread() was not called before");
    gtl_pArena->freeAll();
    gtl_pArena = nullptr;
}

inline ThreadPool::ThreadLocalArena*
ThreadPool::arena() noexcept
{
    return gtl_pArena;
}

/* Usage example:
 * Vec<IThreadPool::Future<Span<f32>>*> vFutures = parallelFor(&arena, &tp, Span<f32> {v},
 *     [](Span<f32> spBatch, isize offFrom0)
 *     {
 *         for (auto& e : spBatch) r += 1 + offFrom0;
 *     }
 * );
 * 
 *  for (auto* pF : vFutures) pF->wait(); */
template<typename THREAD_POOL_T, typename T, typename CL_PROC_BATCH>
[[nodiscard]] inline Vec<IThreadPool::Future<Span<T>>*>
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
        IThreadPool::Future<Span<T>> future {};
        Span<T> sp {};
        isize off {};
        decltype(clProcBatch) cl {};
    };

    Vec<IThreadPool::Future<Span<T>>*> vFutures {pArena, tailSize > 0 ? nThreads + 1 : nThreads};

    auto clBatch = [&](isize off, isize size) {
        Arg* pArg = pArena->alloc<Arg>(pTp, Span<T> {spData.data() + off, size}, off, clProcBatch);
        pArg->future.data() = pArg->sp;

        vFutures.push(pArena, &pArg->future);

        pTp->addRetry([pArg] {
            pArg->cl(pArg->sp, pArg->off);
            pArg->future.signal();
        });
    };

    isize i = 0;
    for (; i < batchSize*nThreads; i += batchSize) clBatch(i, batchSize);
    if (tailSize > 0) clBatch(i, tailSize);

    return vFutures;
}

} /* namespace adt */

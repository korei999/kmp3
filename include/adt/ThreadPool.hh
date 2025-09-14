#pragma once

#include "Arena.hh"
#include "FuncBuffer.hh"
#include "Queue.hh"
#include "StdAllocator.hh"
#include "Thread.hh"
#include "Vec.hh"
#include "atomic.hh"
#include "defer.hh"

namespace adt
{

struct IThreadPool
{
    /* TODO: buffer size can be moved to implementation */
    using Task = FuncBuffer<void, 56>;
    static_assert(sizeof(Task) == 64);

    template<typename T>
    struct Future : adt::Future<T>
    {
        using Base = adt::Future<T>;

        /* */

        IThreadPool* m_pPool {};

        /* */

        Future() noexcept = default;
        Future(IThreadPool* pPool) noexcept;

        /* */

        void wait() noexcept;
        decltype(auto) waitData() noexcept; /* decltype(auto) for the <void> case. */
    };

    /* */

    static int
    optimalThreadCount() noexcept
    {
        static const int s_count = utils::max(ADT_GET_NPROCS() - 1, 1);
        return s_count;
    }

    /* */

    virtual const atomic::Int& nActiveTasks() const noexcept = 0;

    virtual int nThreads() const noexcept = 0;

    virtual bool addTask(void (*pfn)(void*), void* pArg, isize argSize) noexcept = 0;

    virtual void wait(bool bHelp) noexcept = 0; /* bHelp: try to call leftover tasks on waiting thread. */

    virtual Task tryStealTask() noexcept = 0;

    virtual int threadId() noexcept = 0;

    virtual Arena* arena() noexcept = 0;

    template<typename CL>
    bool
    add(const CL& cl) noexcept
    {
        return addTask([](void* p) {
            static_cast<CL*>(p)->operator()();
        }, (void*)&cl, sizeof(cl));
    }

    template<typename T, typename CL>
    bool
    add(Future<T>* pFut, const CL& cl) noexcept
    {
        auto cl2 = [pFut, cl]
        {
            if constexpr (std::is_same_v<void, T>)
            {
                cl();
                pFut->signal();
            }
            else
            {
                pFut->signalData(cl());
            }
        };

        static_assert(sizeof(cl2) >= sizeof(cl) + sizeof(pFut));

        return addTask([](void* p) {
            static_cast<decltype(cl2)*>(p)->operator()();
        }, &cl2, sizeof(cl2));
    }

    template<typename CL>
    void addRetry(const CL& cl) noexcept { while (!add(cl)); }

    template<typename CL>
    bool
    addRetry(const CL& cl, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            if (add(cl)) return true;
        return false;
    }

    template<typename T, typename CL>
    void addRetry(Future<T>* pFut, const CL& cl) noexcept { while (!add(pFut, cl)); }

    template<typename T, typename CL>
    bool
    addRetry(Future<T>* pFut, const CL& cl, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            if (add(pFut, cl)) return true;
        return false;
    }

    template<typename CL>
    void
    addRetryOrDo(const CL& cl) noexcept
    {
        if (nActiveTasks().load(atomic::ORDER::ACQUIRE) >= nThreads()) cl();
        else addRetry(cl);
    }

    template<typename CL>
    bool
    addRetryOrDo(const CL& cl, int n) noexcept
    {
        if (nActiveTasks().load(atomic::ORDER::ACQUIRE) >= nThreads())
        {
            cl();
            return true;
        }
        else
        {
            return addRetry(cl, n);
        }
    }

    template<typename T, typename CL>
    void
    addRetryOrDo(Future<T>* pFut, const CL& cl) noexcept
    {
        if (nActiveTasks().load(atomic::ORDER::ACQUIRE) >= nThreads()) cl();
        else addRetry(pFut, cl);
    }

    template<typename T, typename CL>
    bool
    addRetryOrDo(Future<T>* pFut, const CL& cl, int n) noexcept
    {
        if (nActiveTasks().load(atomic::ORDER::ACQUIRE) >= nThreads())
        {
            cl();
            return true;
        }
        else
        {
            return addRetry(pFut, cl, n);
        }
    }
};

template<typename T>
inline
IThreadPool::Future<T>::Future(IThreadPool* pPool) noexcept
    : Base {INIT}, m_pPool {pPool}
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

    /* */

    static inline thread_local int gtl_threadId {};
    static inline thread_local Arena gtl_arena {};

    /* */

    ThreadPool() = default;

    ThreadPool(isize qSize, isize arenaReserve, int nThreads = optimalThreadCount());

    ThreadPool(
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

    virtual int threadId() noexcept override;

    virtual Arena* arena() noexcept override;

    /* */

    void destroy() noexcept;

protected:
    void start();
    THREAD_STATUS loop();
};

inline
ThreadPool::ThreadPool(isize qSize, isize arenaReserve, int nThreads)
    : m_spThreads(StdAllocator::inst()->zallocV<Thread>(nThreads), nThreads),
      m_mtxQ(Mutex::TYPE::PLAIN),
      m_cndQ(INIT),
      m_cndWait(INIT),
      m_qTasks(qSize),
      m_arenaReserved(arenaReserve)
{
    start();
}

inline
ThreadPool::ThreadPool(
    void (*pfnOnLoopStart)(void*), void* pLoopStartArg,
    void (*pfnOnLoopEnd)(void*), void* pLoopEndArg,
    isize qSize,
    isize arenaReserve,
    int nThreads
)
    : m_spThreads(StdAllocator::inst()->zallocV<Thread>(nThreads), nThreads),
      m_mtxQ(Mutex::TYPE::PLAIN),
      m_cndQ(INIT),
      m_cndWait(INIT),
      m_pfnLoopStart(pfnOnLoopStart),
      m_pLoopStartArg(pLoopStartArg),
      m_pfnLoopEnd(pfnOnLoopEnd),
      m_pLoopEndArg(pLoopEndArg),
      m_qTasks(qSize),
      m_arenaReserved(arenaReserve)
{
    start();
}

inline THREAD_STATUS
ThreadPool::loop()
{
    if (m_pfnLoopStart) m_pfnLoopStart(m_pLoopStartArg);
    ADT_DEFER( if (m_pfnLoopEnd) m_pfnLoopEnd(m_pLoopEndArg) );

    gtl_threadId = 1 + m_atomIdCounter.fetchAdd(1, atomic::ORDER::RELAXED);
    new(&gtl_arena) Arena {m_arenaReserved};
    ADT_DEFER( gtl_arena.freeAll() );

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
    for (auto& thread : m_spThreads)
    {
        thread = Thread(
            reinterpret_cast<ThreadFn>(methodPointerNonVirtual(&ThreadPool::loop)),
            this
        );
    }

    new(&gtl_arena) Arena {m_arenaReserved};

    m_bStarted = true;

#ifndef NDEBUG
    print::err("[ThreadPool]: new pool with {} threads\n", m_spThreads.size());
#endif
}

inline void
ThreadPool::wait(bool bHelp) noexcept
{
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
    wait(true);

    {
        LockScope qLock {&m_mtxQ};
        m_atomBDone.store(true, atomic::ORDER::RELEASE);
    }

    m_cndQ.broadcast();

    for (auto& thread : m_spThreads)
        thread.join();

    ADT_ASSERT(m_atomNActiveTasks.load(atomic::ORDER::ACQUIRE) == 0, "{}", m_atomNActiveTasks.load(atomic::ORDER::RELAXED));

    StdAllocator::inst()->free(m_spThreads.data());
    m_qTasks.destroy();
    m_mtxQ.destroy();
    m_cndQ.destroy();
    m_cndWait.destroy();

    gtl_arena.freeAll();
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

inline int
ThreadPool::threadId() noexcept
{
    return gtl_threadId;
}

inline Arena*
ThreadPool::arena() noexcept
{
    return &gtl_arena;
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

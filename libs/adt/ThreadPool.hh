#pragma once

#include "Thread.hh"
#include "defer.hh"
#include "atomic.hh"
#include "QueueArray.hh"

namespace adt
{

struct ThreadPoolTask
{
    ThreadFn pfn {};
    void* pArg {};
};

template<ssize QUEUE_SIZE>
struct ThreadPool
{
    IAllocator* m_pAlloc {}; /* managed by default */
    QueueArray<ThreadPoolTask, QUEUE_SIZE> m_qTasks {};
    Span<Thread> m_spThreads {};
    Mutex m_mtxQ {};
    CndVar m_cndQ {};
    CndVar m_cndWait {};
    void (*m_pfnLoopStart)(void*) {};
    void* m_pLoopStartArg {};
    void (*m_pfnLoopEnd)(void*) {};
    void* m_pLoopEndArg {};
    atomic::Int m_atom_nActiveTasks {};
    atomic::Int m_atom_bDone {};

    /* */

    ThreadPool() = default;

    ThreadPool(IAllocator* pAlloc, int nThreads = ADT_GET_NPROCS());

    ThreadPool(
        IAllocator* pAlloc,
        void (*pfnLoopStart)(void*), /* call on entering the loop */ 
        void* pLoopStartArg,
        void (*pfnLoopEnd)(void*), /* call on exiting the loop */
        void* pLoopEndArg,
        int nThreads = ADT_GET_NPROCS()
    );

    /* */

    void wait();

    void destroy();

    bool add(ThreadPoolTask task);

    bool add(ThreadFn pfn, void* pArg);

    template<typename LAMBDA> bool addLambda(LAMBDA& t);

    template<typename LAMBDA> requires(std::is_rvalue_reference_v<LAMBDA&&>)
    [[deprecated("rvalue lambdas cause use after free")]] bool addLambda(LAMBDA&& t) = delete;

    void addRetry(ThreadPoolTask task) { while (!add(task)); }

    void addRetry(ThreadFn pfn, void* pArg) { while (!add(pfn, pArg)); }

    template<typename LAMBDA> void addLambdaRetry(LAMBDA&& t) { while (!addLambda(std::forward<LAMBDA>(t))); }

protected:
    THREAD_STATUS loop();
    void spawnThreads();
};

template<ssize QUEUE_SIZE>
inline
ThreadPool<QUEUE_SIZE>::ThreadPool(IAllocator* pAlloc, int nThreads)
    : m_pAlloc(pAlloc),
      m_spThreads(pAlloc->zallocV<Thread>(nThreads), nThreads),
      m_mtxQ(Mutex::TYPE::PLAIN),
      m_cndQ(INIT),
      m_cndWait(INIT),
      m_atom_bDone(false)
{
    spawnThreads();
}

template<ssize QUEUE_SIZE>
inline
ThreadPool<QUEUE_SIZE>::ThreadPool(
    IAllocator* pAlloc,
    void (*pfnLoopStart)(void*), void* pLoopStartArg,
    void (*pfnLoopEnd)(void*), void* pLoopEndArg,
    int nThreads
)
    : m_pAlloc(pAlloc),
      m_spThreads(pAlloc->zallocV<Thread>(nThreads), nThreads),
      m_mtxQ(Mutex::TYPE::PLAIN),
      m_cndQ(INIT),
      m_cndWait(INIT),
      m_pfnLoopStart(pfnLoopStart),
      m_pLoopStartArg(pLoopStartArg),
      m_pfnLoopEnd(pfnLoopEnd),
      m_pLoopEndArg(pLoopEndArg),
      m_atom_bDone(false)
{
    spawnThreads();
}

template<ssize QUEUE_SIZE>
inline THREAD_STATUS
ThreadPool<QUEUE_SIZE>::loop()
{
    if (m_pfnLoopStart) m_pfnLoopStart(m_pLoopStartArg);
    ADT_DEFER( if (m_pfnLoopEnd) m_pfnLoopEnd(m_pLoopEndArg) );

    while (true)
    {
        ThreadPoolTask task {};

        {
            MutexGuard qLock(&m_mtxQ);

            while (m_qTasks.empty() && !m_atom_bDone.load(atomic::ORDER::RELAXED))
                m_cndQ.wait(&m_mtxQ);

            if (m_atom_bDone.load(atomic::ORDER::RELAXED))
                return 0;

            task = *m_qTasks.popFront();
            m_atom_nActiveTasks.fetchAdd(1, atomic::ORDER::SEQ_CST);
        }

        task.pfn(task.pArg);
        m_atom_nActiveTasks.fetchSub(1, atomic::ORDER::SEQ_CST);

        {
            MutexGuard qLock(&m_mtxQ);

            if (m_qTasks.empty() && m_atom_nActiveTasks.load(atomic::ORDER::ACQUIRE) == 0)
                m_cndWait.signal();
        }
    }

    return 0;
}

template<ssize QUEUE_SIZE>
inline void
ThreadPool<QUEUE_SIZE>::spawnThreads()
{
    for (auto& thread : m_spThreads)
    {
        thread = Thread(
            reinterpret_cast<ThreadFn>(methodPointer(&ThreadPool::loop)),
            this
        );
    }

#ifndef NDEBUG
    print::err("ThreadPool: new pool with {} threads\n", m_spThreads.size());
#endif
}

template<ssize QUEUE_SIZE>
inline void
ThreadPool<QUEUE_SIZE>::wait()
{
    MutexGuard qLock(&m_mtxQ);

    while (!m_qTasks.empty() || m_atom_nActiveTasks.load(atomic::ORDER::RELAXED) != 0)
        m_cndWait.wait(&m_mtxQ);
}

template<ssize QUEUE_SIZE>
inline void
ThreadPool<QUEUE_SIZE>::destroy()
{
    wait();

    {
        MutexGuard qLock(&m_mtxQ);
        m_atom_bDone.store(1, atomic::ORDER::RELAXED);
    }

    m_cndQ.broadcast();

    for (auto& thread : m_spThreads)
        thread.join();

    {
        m_pAlloc->free(m_spThreads.data());
        m_mtxQ.destroy();
        m_cndQ.destroy();
        m_cndWait.destroy();
    }
}

template<ssize QUEUE_SIZE>
inline bool
ThreadPool<QUEUE_SIZE>::add(ThreadPoolTask task)
{
    MutexGuard lock(&m_mtxQ);

    ssize i = m_qTasks.pushBack(task);
    m_cndQ.signal();

    return i;
}

template<ssize QUEUE_SIZE>
inline bool
ThreadPool<QUEUE_SIZE>::add(ThreadFn pfn, void* pArg)
{
    return add({pfn, pArg});
}

template<ssize QUEUE_SIZE>
template<typename LAMBDA>
inline bool
ThreadPool<QUEUE_SIZE>::addLambda(LAMBDA& t)
{
    return add(+[](void* pArg) -> THREAD_STATUS
        {
            reinterpret_cast<LAMBDA*>(pArg)->operator()();
            return 0;
        },
        (void*)(&t)
    );
}

} /* namespace adt */

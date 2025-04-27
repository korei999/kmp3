#pragma once

#include "Thread.hh"
#include "defer.hh"
#include "atomic.hh"
#include "QueueArray.hh"

namespace adt
{

template<ssize QUEUE_SIZE>
struct ThreadPool
{
    struct Task
    {
        ThreadFn pfn {};
        void* pArg {};
    };

    /* */

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
    QueueArray<Task, QUEUE_SIZE> m_qTasks {};

    /* */

    static inline thread_local int inl_threadId {};

    /* */

    ThreadPool() = default;

    ThreadPool(IAllocator* pAlloc, int nThreads = ADT_GET_NPROCS());

    ThreadPool(
        IAllocator* pAlloc,
        void (*pfnOnLoopStart)(void*),
        void* pLoopStartArg,
        void (*pfnOnLoopEnd)(void*),
        void* pLoopEndArg,
        int nThreads = ADT_GET_NPROCS()
    );

    /* */

    void wait();

    void destroy(IAllocator* pAlloc);

    bool add(Task task);

    bool add(ThreadFn pfn, void* pArg);

    template<typename LAMBDA> bool addLambda(LAMBDA& t);

    template<typename LAMBDA> requires(std::is_rvalue_reference_v<LAMBDA&&>)
    [[deprecated("rvalue lambdas cause use after free")]] bool addLambda(LAMBDA&& t) = delete;

    void addRetry(Task task) { while (!add(task)); }

    void addRetry(ThreadFn pfn, void* pArg) { while (!add(pfn, pArg)); }

    template<typename LAMBDA> requires(std::is_rvalue_reference_v<LAMBDA&&>)
    [[deprecated("rvalue lambdas cause use after free")]] void addLambdaRetry(LAMBDA&& t) = delete;

protected:
    THREAD_STATUS loop();
    void spawnThreads();
};

template<ssize QUEUE_SIZE>
inline
ThreadPool<QUEUE_SIZE>::ThreadPool(IAllocator* pAlloc, int nThreads)
    : m_spThreads(pAlloc->zallocV<Thread>(nThreads), nThreads),
      m_mtxQ(Mutex::TYPE::PLAIN),
      m_cndQ(INIT),
      m_cndWait(INIT),
      m_atomBDone(false)
{
    spawnThreads();
}

template<ssize QUEUE_SIZE>
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
    spawnThreads();
}

template<ssize QUEUE_SIZE>
inline THREAD_STATUS
ThreadPool<QUEUE_SIZE>::loop()
{
    if (m_pfnLoopStart) m_pfnLoopStart(m_pLoopStartArg);
    ADT_DEFER( if (m_pfnLoopEnd) m_pfnLoopEnd(m_pLoopEndArg) );

    inl_threadId = 1 + m_atomIdCounter.fetchAdd(1, atomic::ORDER::RELAXED);

    while (true)
    {
        Task task {};

        {
            LockGuard qLock {&m_mtxQ};

            while (m_qTasks.empty() && !m_atomBDone.load(atomic::ORDER::ACQUIRE))
                m_cndQ.wait(&m_mtxQ);

            if (m_atomBDone.load(atomic::ORDER::ACQUIRE))
                return 0;

            task = *m_qTasks.popFront();
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
    LockGuard qLock {&m_mtxQ};

    while (!m_qTasks.empty() || m_atomNActiveTasks.load(atomic::ORDER::RELAXED) != 0)
        m_cndWait.wait(&m_mtxQ);
}

template<ssize QUEUE_SIZE>
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

template<ssize QUEUE_SIZE>
inline bool
ThreadPool<QUEUE_SIZE>::add(Task task)
{
    ssize i;

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

#pragma once

#include "Thread.hh"
#include "Queue.hh"
#include "Span.hh"
#include "defer.hh"
#include "atomic.hh"

namespace adt
{

struct ThreadPoolTask
{
    ThreadFn pfn {};
    void* pArg {};
};

struct ThreadPool
{
    IAllocator* m_pAlloc {}; /* managed by default */
    Queue<ThreadPoolTask> m_qTasks {};
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
        void (*pfnLoopStart)(void*),
        void* pLoopStartArg,
        void (*pfnLoopEnd)(void*),
        void* pLoopEndArg,
        int nThreads = ADT_GET_NPROCS()
    );

    /* */

    void wait();
    void destroy();
    void add(ThreadPoolTask task);
    void add(ThreadFn pfn, void* pArg);
    template<typename LAMBDA> void addLambda(LAMBDA& t);

    template<typename LAMBDA> requires(std::is_rvalue_reference_v<LAMBDA&&>)
    [[deprecated("rvalue lambdas cause use after free")]] void addLambda(LAMBDA&& t) = delete;

protected:
    THREAD_STATUS loop();
    void spawnThreads();
};

inline
ThreadPool::ThreadPool(IAllocator* pAlloc, int nThreads)
    : m_pAlloc(pAlloc),
      m_qTasks(pAlloc, nThreads * 2),
      m_spThreads(pAlloc->zallocV<Thread>(nThreads), nThreads),
      m_mtxQ(Mutex::TYPE::PLAIN),
      m_cndQ(INIT),
      m_cndWait(INIT),
      m_atom_bDone(false)
{
    spawnThreads();
}

inline
ThreadPool::ThreadPool(
    IAllocator* pAlloc,
    void (*pfnLoopStart)(void*), void* pLoopStartArg,
    void (*pfnLoopEnd)(void*), void* pLoopEndArg,
    int nThreads
)
    : m_pAlloc(pAlloc),
      m_qTasks(pAlloc, nThreads * 2),
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

inline THREAD_STATUS
ThreadPool::loop()
{
    if (m_pfnLoopStart) m_pfnLoopStart(m_pLoopStartArg);
    defer( if (m_pfnLoopEnd) m_pfnLoopEnd(m_pLoopEndArg) );

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

inline void
ThreadPool::spawnThreads()
{
    for (auto& thread : m_spThreads)
    {
        thread = Thread(
            reinterpret_cast<ThreadFn>(methodPointer(&ThreadPool::loop)),
            this
        );
    }

#ifndef NDEBUG
    fprintf(stderr, "ThreadPool: new pool with %lld threads\n", m_spThreads.size());
#endif
}

inline void
ThreadPool::wait()
{
    MutexGuard qLock(&m_mtxQ);

    while (!m_qTasks.empty() || m_atom_nActiveTasks.load(atomic::ORDER::RELAXED) != 0)
        m_cndWait.wait(&m_mtxQ);
}

inline void
ThreadPool::destroy()
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
        m_qTasks.destroy(m_pAlloc);
        m_pAlloc->free(m_spThreads.data());
        m_mtxQ.destroy();
        m_cndQ.destroy();
        m_cndWait.destroy();
    }
}

inline void
ThreadPool::add(ThreadPoolTask task)
{
    MutexGuard lock(&m_mtxQ);

    m_qTasks.pushBack(m_pAlloc, task);
    m_cndQ.signal();
}

inline void
ThreadPool::add(ThreadFn pfn, void* pArg)
{
    add({pfn, pArg});
}

template<typename LAMBDA>
inline void
ThreadPool::addLambda(LAMBDA& t)
{
    add(+[](void* pArg) -> THREAD_STATUS
        {
            reinterpret_cast<LAMBDA*>(pArg)->operator()();
            return 0;
        },
        (void*)(&t)
    );
}

} /* namespace adt */

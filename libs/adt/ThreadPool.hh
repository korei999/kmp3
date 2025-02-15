#pragma once

#include "Thread.hh"
#include "Queue.hh"
#include "Span.hh"
#include "guard.hh"

#include <atomic>

namespace adt
{

struct ThreadPoolTask
{
    ThreadFn pfn {};
    void* pArg {};
};

struct ThreadPool
{
    IAllocator* m_pAlloc {};
    QueueBase<ThreadPoolTask> m_qTasks {};
    Span<Thread> m_spThreads {};
    Mutex m_mtxQ {};
    CndVar m_cndQ {};
    CndVar m_cndWait {};
    std::atomic<int> m_nActiveTasks {};
    std::atomic<bool> m_bDone {};

    /* */

    ThreadPool() = default;
    ThreadPool(IAllocator* pAlloc, int nThreads = ADT_GET_NPROCS());

    /* */

    void wait();
    void destroy();
    void add(ThreadPoolTask task);
    void add(ThreadFn pfn, void* pArg);
    template<typename LAMBDA> void add(LAMBDA t);

private:
    THREAD_STATUS loop(void* pArg);
};

inline
ThreadPool::ThreadPool(IAllocator* pAlloc, int nThreads)
    : m_pAlloc(pAlloc),
      m_qTasks(pAlloc, nThreads * 2),
      m_spThreads(pAlloc->zallocV<Thread>(nThreads), nThreads),
      m_mtxQ(MUTEX_TYPE::PLAIN),
      m_cndQ(INIT),
      m_cndWait(INIT),
      m_bDone(false)
{
    for (auto& thread : m_spThreads)
    {
        thread = Thread(
            reinterpret_cast<ThreadFn>(methodPointer(&ThreadPool::loop)),
            this
        );
    }
}

inline THREAD_STATUS
ThreadPool::loop(void* pArg)
{
    while (true)
    {
        ThreadPoolTask task {};

        {
            guard::Mtx qLock(&m_mtxQ);

            while (m_qTasks.empty() && !m_bDone.load(std::memory_order_relaxed))
                m_cndQ.wait(&m_mtxQ);

            if (m_bDone.load(std::memory_order_relaxed))
                return 0;

            task = *m_qTasks.popFront();
            m_nActiveTasks.fetch_add(1, std::memory_order_seq_cst);
        }

        task.pfn(task.pArg);
        m_nActiveTasks.fetch_sub(1,std::memory_order_seq_cst);

        {
            guard::Mtx qLock(&m_mtxQ);

            if (m_qTasks.empty() && m_nActiveTasks.load(std::memory_order_acquire) == 0)
                m_cndWait.signal();
        }
    }

    return 0;
}

inline void
ThreadPool::wait()
{
    guard::Mtx qLock(&m_mtxQ);

    while (!m_qTasks.empty() || m_nActiveTasks.load(std::memory_order_relaxed) != 0)
        m_cndWait.wait(&m_mtxQ);
}

inline void
ThreadPool::destroy()
{
    wait();

    {
        guard::Mtx qLock(&m_mtxQ);
        m_bDone.store(true, std::memory_order_relaxed);
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
    guard::Mtx lock(&m_mtxQ);

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
ThreadPool::add(LAMBDA t)
{
    add(+[](void* pArg) -> THREAD_STATUS
        {
            (*reinterpret_cast<decltype(t)*>(pArg))();
            return 0;
        },
        &t
    );
}

} /* namespace adt */

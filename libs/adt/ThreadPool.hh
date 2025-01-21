#pragma once

#include "Queue.hh"
#include "adt/Vec.hh"
#include "defer.hh"
#include "guard.hh"
#include "Thread.hh"

#include <atomic>
#include <cstdio>

namespace adt
{

#ifdef __linux__
    #include <sys/sysinfo.h>

    #define ADT_GET_NCORES() get_nprocs()
#elif _WIN32
    #define WIN32_LEAN_AND_MEAN 1
    #include <windows.h>
    #ifdef min
        #undef min
    #endif
    #ifdef max
        #undef max
    #endif
    #ifdef near
        #undef near
    #endif
    #ifdef far
        #undef far
    #endif
    #include <sysinfoapi.h>

inline DWORD
getLogicalCoresCountWIN32()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

    #define ADT_GET_NCORES() getLogicalCoresCountWIN32()
#else
    #define ADT_GET_NCORES() 4
#endif

inline int
getNCores()
{
#ifdef __linux__
    return get_nprocs();
#elif _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;

    return info.dwNumberOfProcessors;
#endif
    return 4;
}

enum class WAIT_FLAG : u8 { DONT_WAIT, WAIT };

/* wait for individual task completion without ThreadPoolWait */
struct ThreadPoolLock
{
    std::atomic<bool> m_bSignaled;
    Mutex m_mtx {};
    CndVar m_cnd {};

    /* */

    ThreadPoolLock() = default;
    ThreadPoolLock(INIT_FLAG) { init(); }

    /* */

    void init();
    void wait();
    void destroy();
};

inline void
ThreadPoolLock::init()
{
    m_bSignaled.store(false, std::memory_order_relaxed);
    m_mtx = Mutex(MUTEX_TYPE::PLAIN);
    m_cnd = CndVar(INIT);
}

inline void
ThreadPoolLock::wait()
{
    guard::Mtx lock(&m_mtx);
    m_cnd.wait(&m_mtx);
    /* notify thread pool's spinlock that we have woken up */
    m_bSignaled.store(true, std::memory_order_relaxed);
}

inline void
ThreadPoolLock::destroy()
{
    m_mtx.destroy();
    m_cnd.destroy();
}

struct ThreadTask
{
    ThreadFn pfn {};
    void* pArgs {};
    WAIT_FLAG eWait {};
    ThreadPoolLock* pLock {};
};

struct ThreadPool
{
    IAllocator* m_pAlloc {};
    QueueBase<ThreadTask> m_qTasks {};
    VecBase<Thread> m_aThreads {};
    CndVar m_cndQ {}, m_cndWait {};
    Mutex m_mtxQ {}, m_mtxWait {};
    std::atomic<int> m_nActiveTasks {};
    std::atomic<int> m_nActiveThreadsInLoop {};
    std::atomic<bool> m_bDone {};
    bool m_bStarted {};

    /* */

    ThreadPool() = default;
    ThreadPool(IAllocator* pAlloc, u32 _nThreads = ADT_GET_NCORES());

    /* */

    void destroy();
    void start();
    bool busy();
    void submit(ThreadTask task);
    void submit(ThreadFn pfnTask, void* pArgs);
    /* Signal ThreadPoolLock after completion.
     * If `ThreadPoolLock::wait()` was never called for this `pTpLock`, the task will spinlock forever,
     * unless `pTpLock->bSignaled` is manually set to true; */
    void submitSignal(ThreadFn pfnTask, void* pArgs, ThreadPoolLock* pTpLock);
    void wait(); /* wait for all active tasks to finish, without joining */
};

inline
ThreadPool::ThreadPool(IAllocator* _pAlloc, u32 _nThreads)
    : m_pAlloc(_pAlloc),
      m_qTasks(_pAlloc, _nThreads),
      m_aThreads(_pAlloc, _nThreads),
      m_nActiveTasks(0),
      m_nActiveThreadsInLoop(0),
      m_bDone(true),
      m_bStarted(false)
{
    assert(_nThreads != 0 && "can't have thread pool with zero threads");
    m_aThreads.setSize(_pAlloc, _nThreads);

    m_cndQ = CndVar(INIT);
    m_mtxQ = Mutex(MUTEX_TYPE::PLAIN);
    m_cndWait = CndVar(INIT);
    m_mtxWait = Mutex(MUTEX_TYPE::PLAIN);
}

inline THREAD_STATUS
_ThreadPoolLoop(void* p)
{
    auto* s = (ThreadPool*)p;

    s->m_nActiveThreadsInLoop.fetch_add(1, std::memory_order_relaxed);
    defer( s->m_nActiveThreadsInLoop.fetch_sub(1, std::memory_order_relaxed) );

    while (!s->m_bDone)
    {
        ThreadTask task;
        {
            guard::Mtx lock(&s->m_mtxQ);

            while (s->m_qTasks.empty() && !s->m_bDone)
                s->m_cndQ.wait(&s->m_mtxQ);

            if (s->m_bDone) return {};

            task = *s->m_qTasks.popFront();
            /* increment before unlocking mtxQ to avoid 0 tasks and 0 q possibility */
            s->m_nActiveTasks.fetch_add(1, std::memory_order_relaxed);
        }

        task.pfn(task.pArgs);
        s->m_nActiveTasks.fetch_sub(1, std::memory_order_relaxed);

        if (task.eWait == WAIT_FLAG::WAIT)
        {
            /* keep signaling until it's truly awakened */
            while (task.pLock->m_bSignaled.load(std::memory_order_relaxed) == false)
                task.pLock->m_cnd.signal();
        }

        if (!s->busy())
            s->m_cndWait.signal();
    }

    return {};
}

inline void
ThreadPool::start()
{
    m_bStarted = true;
    m_bDone.store(false, std::memory_order_relaxed);

#ifndef NDEBUG
    fprintf(stderr, "[ThreadPool]: staring %lld threads\n", m_aThreads.getSize());
#endif

    for (auto& thread : m_aThreads)
        thread = Thread(_ThreadPoolLoop, this);
}

inline bool
ThreadPool::busy()
{
    bool ret;
    {
        guard::Mtx lock(&m_mtxQ);
        ret = !m_qTasks.empty() || m_nActiveTasks > 0;
    }

    return ret;
}

inline void
ThreadPool::submit(ThreadTask task)
{
    {
        guard::Mtx lock(&m_mtxQ);
        m_qTasks.pushBack(m_pAlloc, task);
    }

    m_cndQ.signal();
}

inline void
ThreadPool::submit(ThreadFn pfnTask, void* pArgs)
{
    assert(m_bStarted && "[ThreadPool]: never called ThreadPoolStart()");

    submit({pfnTask, pArgs});
}

inline void
ThreadPool::submitSignal(ThreadFn pfnTask, void* pArgs, ThreadPoolLock* pTpLock)
{
    submit({pfnTask, pArgs, WAIT_FLAG::WAIT, pTpLock});
}

inline void
ThreadPool::wait()
{
    assert(m_bStarted && "[ThreadPool]: never called ThreadPoolStart()");

    while (busy())
    {
        guard::Mtx lock(&m_mtxWait);
        m_cndWait.wait(&m_mtxWait);
    }
}

inline void
_ThreadPoolStop(ThreadPool* s)
{
    s->m_bStarted = false;

    if (s->m_bDone)
    {
#ifndef NDEBUG
        fprintf(stderr, "[ThreadPool]: trying to stop multiple times or stopping without starting at all\n");
#endif
        return;
    }

    atomic_store(&s->m_bDone, true);

    /* some threads might not cnd_wait() in time, so keep signaling untill all return from the loop */
    while (s->m_nActiveThreadsInLoop.load(std::memory_order_relaxed) > 0)
        s->m_cndQ.broadcast();

    for (auto& thread : s->m_aThreads)
        thread.join();
}

inline void
ThreadPool::destroy()
{
    _ThreadPoolStop(this);

    m_aThreads.destroy(m_pAlloc);
    m_qTasks.destroy(m_pAlloc);

    m_cndQ.destroy();
    m_mtxQ.destroy();
    m_cndWait.destroy();
    m_mtxWait.destroy();
}

} /* namespace adt */

#pragma once

#include "Queue.hh"
#include "Vec.hh"
#include "defer.hh"
#include "guard.hh"

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
    mtx_t m_mtx;
    cnd_t m_cnd;

    /* */

    ThreadPoolLock() = default;
    ThreadPoolLock([[maybe_unused]] INIT_FLAG e) { init(); }

    /* */

    void init();
    void wait();
    void destroy();
};

inline void
ThreadPoolLock::init()
{
    m_bSignaled.store(false, std::memory_order_relaxed);
    mtx_init(&m_mtx, mtx_plain);
    cnd_init(&m_cnd);
}

inline void
ThreadPoolLock::wait()
{
    guard::Mtx lock(&m_mtx);
    cnd_wait(&m_cnd, &m_mtx);
    /* notify thread pool's spinlock that we have woken up */
    m_bSignaled.store(true, std::memory_order_relaxed);
}

inline void
ThreadPoolLock::destroy()
{
    mtx_destroy(&m_mtx);
    cnd_destroy(&m_cnd);
}

struct ThreadTask
{
    thrd_start_t pfn {};
    void* pArgs {};
    WAIT_FLAG eWait {};
    ThreadPoolLock* pLock {};
};

struct ThreadPool
{
    IAllocator* m_pAlloc {};
    QueueBase<ThreadTask> m_qTasks {};
    VecBase<thrd_t> m_aThreads {};
    cnd_t m_cndQ {}, m_cndWait {};
    mtx_t m_mtxQ {}, m_mtxWait {};
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
    void submit(thrd_start_t pfnTask, void* pArgs);
    /* Signal ThreadPoolLock after completion.
     * If `ThreadPoolLock::wait()` was never called for this `pTpLock`, the task will spinlock forever,
     * unless `pTpLock->bSignaled` is manually set to true; */
    void submitSignal(thrd_start_t pfnTask, void* pArgs, ThreadPoolLock* pTpLock);
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

    cnd_init(&m_cndQ);
    mtx_init(&m_mtxQ, mtx_plain);
    cnd_init(&m_cndWait);
    mtx_init(&m_mtxWait, mtx_plain);
}

inline int
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
                cnd_wait(&s->m_cndQ, &s->m_mtxQ);

            if (s->m_bDone) return thrd_success;

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
                cnd_signal(&task.pLock->m_cnd);
        }

        if (!s->busy())
            cnd_signal(&s->m_cndWait);
    }

    return thrd_success;
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
    {
        [[maybe_unused]] int t = thrd_create(&thread, _ThreadPoolLoop, this);
#ifndef NDEBUG
        assert(t == 0 && "failed to create thread");
#endif
    }
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

    cnd_signal(&m_cndQ);
}

inline void
ThreadPool::submit(thrd_start_t pfnTask, void* pArgs)
{
    assert(m_bStarted && "[ThreadPool]: never called ThreadPoolStart()");

    submit({pfnTask, pArgs});
}

inline void
ThreadPool::submitSignal(thrd_start_t pfnTask, void* pArgs, ThreadPoolLock* pTpLock)
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
        cnd_wait(&m_cndWait, &m_mtxWait);
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
        cnd_broadcast(&s->m_cndQ);

    for (auto& thread : s->m_aThreads)
        thrd_join(thread, nullptr);
}

inline void
ThreadPool::destroy()
{
    _ThreadPoolStop(this);

    m_aThreads.destroy(m_pAlloc);
    m_qTasks.destroy(m_pAlloc);
    cnd_destroy(&m_cndQ);
    mtx_destroy(&m_mtxQ);
    cnd_destroy(&m_cndWait);
    mtx_destroy(&m_mtxWait);
}

} /* namespace adt */

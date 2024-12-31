#pragma once

#include "Queue.hh"
#include "Vec.hh"
#include "defer.hh"
#include "guard.hh"

#include <stdatomic.h>
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

enum class WAIT_FLAG : u64 { DONT_WAIT, WAIT };

/* wait for individual task completion without ThreadPoolWait */
struct ThreadPoolLock
{
    atomic_bool bSignaled;
    mtx_t mtx;
    cnd_t cnd;

    ThreadPoolLock() = default;
    ThreadPoolLock([[maybe_unused]] INIT_FLAG e) { init(); }

    void init();
    void wait();
    void destroy();
};

inline void
ThreadPoolLock::init()
{
    atomic_store_explicit(&this->bSignaled, false, memory_order_relaxed);
    mtx_init(&this->mtx, mtx_plain);
    cnd_init(&this->cnd);
}

inline void
ThreadPoolLock::wait()
{
    guard::Mtx lock(&this->mtx);
    cnd_wait(&this->cnd, &this->mtx);
    /* notify thread pool's spinlock that we have woken up */
    atomic_store_explicit(&this->bSignaled, true, memory_order_relaxed);
}

inline void
ThreadPoolLock::destroy()
{
    mtx_destroy(&this->mtx);
    cnd_destroy(&this->cnd);
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
    IAllocator* pAlloc {};
    QueueBase<ThreadTask> qTasks {};
    VecBase<thrd_t> aThreads {};
    cnd_t cndQ {}, cndWait {};
    mtx_t mtxQ {}, mtxWait {};
    atomic_int nActiveTasks {};
    atomic_int nActiveThreadsInLoop {};
    atomic_bool bDone {};
    bool bStarted {};

    ThreadPool() = default;
    ThreadPool(IAllocator* pAlloc, u32 _nThreads = ADT_GET_NCORES());

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
    : pAlloc(_pAlloc),
      qTasks(_pAlloc, _nThreads),
      aThreads(_pAlloc, _nThreads),
      nActiveTasks(0),
      nActiveThreadsInLoop(0),
      bDone(true),
      bStarted(false)
{
    assert(_nThreads != 0 && "can't have thread pool with zero threads");
    aThreads.setSize(_pAlloc, _nThreads);

    cnd_init(&cndQ);
    mtx_init(&mtxQ, mtx_plain);
    cnd_init(&cndWait);
    mtx_init(&mtxWait, mtx_plain);
}

inline int
_ThreadPoolLoop(void* p)
{
    auto* s = (ThreadPool*)p;

    s->nActiveThreadsInLoop.fetch_add(1, memory_order_relaxed);
    defer( s->nActiveThreadsInLoop.fetch_sub(1, memory_order_relaxed) );

    while (!s->bDone)
    {
        ThreadTask task;
        {
            guard::Mtx lock(&s->mtxQ);

            while (s->qTasks.empty() && !s->bDone)
                cnd_wait(&s->cndQ, &s->mtxQ);

            if (s->bDone) return thrd_success;

            task = *s->qTasks.popFront();
            /* increment before unlocking mtxQ to avoid 0 tasks and 0 q possibility */
            s->nActiveTasks.fetch_add(1, memory_order_relaxed);
        }

        task.pfn(task.pArgs);
        s->nActiveTasks.fetch_sub(1, memory_order_relaxed);

        if (task.eWait == WAIT_FLAG::WAIT)
        {
            /* keep signaling until it's truly awakened */
            while (task.pLock->bSignaled.load(memory_order_relaxed) == false)
                cnd_signal(&task.pLock->cnd);
        }

        if (!s->busy())
            cnd_signal(&s->cndWait);
    }

    return thrd_success;
}

inline void
ThreadPool::start()
{
    this->bStarted = true;
    atomic_store_explicit(&this->bDone, false, memory_order_relaxed);

#ifndef NDEBUG
    fprintf(stderr, "[ThreadPool]: staring %d threads\n", this->aThreads.getSize());
#endif

    for (auto& thread : this->aThreads)
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
        guard::Mtx lock(&this->mtxQ);
        ret = !this->qTasks.empty() || this->nActiveTasks > 0;
    }

    return ret;
}

inline void
ThreadPool::submit(ThreadTask task)
{
    {
        guard::Mtx lock(&this->mtxQ);
        this->qTasks.pushBack(this->pAlloc, task);
    }

    cnd_signal(&this->cndQ);
}

inline void
ThreadPool::submit(thrd_start_t pfnTask, void* pArgs)
{
    assert(this->bStarted && "[ThreadPool]: never called ThreadPoolStart()");

    this->submit({pfnTask, pArgs});
}

inline void
ThreadPool::submitSignal(thrd_start_t pfnTask, void* pArgs, ThreadPoolLock* pTpLock)
{
    this->submit({pfnTask, pArgs, WAIT_FLAG::WAIT, pTpLock});
}

inline void
ThreadPool::wait()
{
    assert(this->bStarted && "[ThreadPool]: never called ThreadPoolStart()");

    while (this->busy())
    {
        guard::Mtx lock(&this->mtxWait);
        cnd_wait(&this->cndWait, &this->mtxWait);
    }
}

inline void
_ThreadPoolStop(ThreadPool* s)
{
    s->bStarted = false;

    if (s->bDone)
    {
#ifndef NDEBUG
        fprintf(stderr, "[ThreadPool]: trying to stop multiple times or stopping without starting at all\n");
#endif
        return;
    }

    atomic_store(&s->bDone, true);

    /* some threads might not cnd_wait() in time, so keep signaling untill all return from the loop */
    while (atomic_load_explicit(&s->nActiveThreadsInLoop, memory_order_relaxed) > 0)
        cnd_broadcast(&s->cndQ);

    for (auto& thread : s->aThreads)
        thrd_join(thread, nullptr);
}

inline void
ThreadPool::destroy()
{
    _ThreadPoolStop(this);

    this->aThreads.destroy(this->pAlloc);
    this->qTasks.destroy(this->pAlloc);
    cnd_destroy(&this->cndQ);
    mtx_destroy(&this->mtxQ);
    cnd_destroy(&this->cndWait);
    mtx_destroy(&this->mtxWait);
}

} /* namespace adt */

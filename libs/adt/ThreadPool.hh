#pragma once

#include "Queue.hh"
#include "Vec.hh"
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
};

inline void
ThreadPoolLockInit(ThreadPoolLock* s)
{
    atomic_store_explicit(&s->bSignaled, false, memory_order_relaxed);
    mtx_init(&s->mtx, mtx_plain);
    cnd_init(&s->cnd);
}

inline void
ThreadPoolLockWait(ThreadPoolLock* s)
{
    guard::Mtx lock(&s->mtx);
    cnd_wait(&s->cnd, &s->mtx);
    /* notify thread pool's spinlock that we have woken up */
    atomic_store_explicit(&s->bSignaled, true, memory_order_relaxed);
}

inline void
ThreadPoolLockDestroy(ThreadPoolLock* s)
{
    mtx_destroy(&s->mtx);
    cnd_destroy(&s->cnd);
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
    Allocator* pAlloc {};
    QueueBase<ThreadTask> qTasks {};
    VecBase<thrd_t> aThreads {};
    cnd_t cndQ {}, cndWait {};
    mtx_t mtxQ {}, mtxWait {};
    atomic_int nActiveTasks {};
    atomic_bool bDone {};

    ThreadPool() = default;
    ThreadPool(Allocator* pAlloc, u32 _nThreads = ADT_GET_NCORES());
};

inline void ThreadPoolStart(ThreadPool* s);
inline bool ThreadPoolBusy(ThreadPool* s);
inline void ThreadPoolSubmit(ThreadPool* s, ThreadTask task);
inline void ThreadPoolSubmit(ThreadPool* s, thrd_start_t pfnTask, void* pArgs);
/* Signal ThreadPoolLock after completion.
 * If ThreadPoolLockWait was never called for this pTpLock, the task will spinlock forever,
 * unless pTpLock->bSignaled is manually set to true; */
inline void ThreadPoolSubmitSignal(ThreadPool* s, thrd_start_t pfnTask, void* pArgs, ThreadPoolLock* pTpLock);
inline void ThreadPoolWait(ThreadPool* s); /* wait for all active tasks to finish, without joining */

inline
ThreadPool::ThreadPool(Allocator* _pAlloc, u32 _nThreads)
    : pAlloc(_pAlloc), qTasks(_pAlloc, _nThreads), aThreads(_pAlloc, _nThreads), nActiveTasks(0), bDone(true)
{
    VecSetSize(&aThreads, _pAlloc, _nThreads);
    cnd_init(&cndQ);
    mtx_init(&mtxQ, mtx_plain);
    cnd_init(&cndWait);
    mtx_init(&mtxWait, mtx_plain);
}

inline int
_ThreadPoolLoop(void* p)
{
    auto* s = (ThreadPool*)p;

    while (!s->bDone)
    {
        ThreadTask task;
        {
            guard::Mtx lock(&s->mtxQ);

            while (utils::empty(&s->qTasks) && !s->bDone)
                cnd_wait(&s->cndQ, &s->mtxQ);

            if (s->bDone) return thrd_success;

            task = *QueuePopFront(&s->qTasks);
            /* increment before unlocking mtxQ to avoid 0 tasks and 0 q possibility */
            atomic_fetch_add_explicit(&s->nActiveTasks, 1, memory_order_relaxed);
        }

        task.pfn(task.pArgs);
        atomic_fetch_sub_explicit(&s->nActiveTasks, 1, memory_order_relaxed);

        if (task.eWait == WAIT_FLAG::WAIT)
        {
            /* keep signaling until it's truly awakaned */
            while (atomic_load_explicit(&task.pLock->bSignaled, memory_order_relaxed) == false)
                cnd_signal(&task.pLock->cnd);
        }

        if (!ThreadPoolBusy(s))
            cnd_signal(&s->cndWait);
    }

    return thrd_success;
}

inline void
ThreadPoolStart(ThreadPool* s)
{
    atomic_store_explicit(&s->bDone, false, memory_order_relaxed);

#ifndef NDEBUG
    fprintf(stderr, "[ThreadPool]: staring %d threads\n", VecSize(&s->aThreads));
#endif

    for (auto& thread : s->aThreads)
    {
        [[maybe_unused]] int t = thrd_create(&thread, _ThreadPoolLoop, s);
#ifndef NDEBUG
        assert(t == 0 && "failed to create thread");
#endif
    }
}

inline bool
ThreadPoolBusy(ThreadPool* s)
{
    bool ret;
    {
        guard::Mtx lock(&s->mtxQ);
        ret = !utils::empty(&s->qTasks) || s->nActiveTasks > 0;
    }

    return ret;
}

inline void
ThreadPoolSubmit(ThreadPool* s, ThreadTask task)
{
    {
        guard::Mtx lock(&s->mtxQ);
        QueuePushBack(&s->qTasks, s->pAlloc, task);
    }

    cnd_signal(&s->cndQ);
}

inline void
ThreadPoolSubmit(ThreadPool* s, thrd_start_t pfnTask, void* pArgs)
{
    ThreadPoolSubmit(s, {pfnTask, pArgs});
}

inline void
ThreadPoolSubmitSignal(ThreadPool* s, thrd_start_t pfnTask, void* pArgs, ThreadPoolLock* pTpLock)
{
    ThreadPoolSubmit(s, {pfnTask, pArgs, WAIT_FLAG::WAIT, pTpLock});
}

inline void
ThreadPoolWait(ThreadPool* s)
{
    while (ThreadPoolBusy(s))
    {
        guard::Mtx lock(&s->mtxWait);
        cnd_wait(&s->cndWait, &s->mtxWait);
    }
}

inline void
_ThreadPoolStop(ThreadPool* s)
{
    if (s->bDone)
    {
#ifndef NDEBUG
        fprintf(stderr, "[ThreadPool]: trying to stop multiple times or stopping without starting at all\n");
#endif
        return;
    }

    atomic_store(&s->bDone, true);
    cnd_broadcast(&s->cndQ);
    for (auto& thread : s->aThreads)
        thrd_join(thread, nullptr);
}

inline void
ThreadPoolDestroy(ThreadPool* s)
{
    _ThreadPoolStop(s);

    VecDestroy(&s->aThreads, s->pAlloc);
    QueueDestroy(&s->qTasks, s->pAlloc);
    cnd_destroy(&s->cndQ);
    mtx_destroy(&s->mtxQ);
    cnd_destroy(&s->cndWait);
    mtx_destroy(&s->mtxWait);
}

} /* namespace adt */

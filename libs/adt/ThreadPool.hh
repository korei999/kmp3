#pragma once

#include "Queue.hh"
#include "guard.hh"

#include <cstdio>
#include <atomic>

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

struct ThreadTask
{
    thrd_start_t pfn;
    void* pArgs;
};

struct ThreadPool
{
    Queue<ThreadTask> qTasks {};
    thrd_t* pThreads = nullptr;
    u32 nThreads = 0;
    cnd_t cndQ, cndWait;
    mtx_t mtxQ, mtxWait;
    std::atomic<int> a_nActiveTasks;
    std::atomic<bool> bDone;

    ThreadPool() = default;
    ThreadPool(Allocator* pAlloc, u32 _nThreads = ADT_GET_NCORES());
};

inline void ThreadPoolStart(ThreadPool* s);
inline bool ThreadPoolBusy(ThreadPool* s);
inline void ThreadPoolSubmit(ThreadPool* s, ThreadTask task);
inline void ThreadPoolSubmit(ThreadPool* s, thrd_start_t pfnTask, void* pArgs);
inline void ThreadPoolWait(ThreadPool* s); /* wait for active tasks to finish, without joining */

inline
ThreadPool::ThreadPool(Allocator* pAlloc, u32 _nThreads)
    : qTasks(pAlloc, _nThreads), nThreads(_nThreads), a_nActiveTasks(0), bDone(true)
{
    pThreads = (thrd_t*)alloc(pAlloc, _nThreads, sizeof(thrd_t));
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

            while (QueueEmpty(&s->qTasks) && !s->bDone)
                cnd_wait(&s->cndQ, &s->mtxQ);

            if (s->bDone) return thrd_success;

            task = *QueuePopFront(&s->qTasks);
            s->a_nActiveTasks++; /* increment before unlocking mtxQ to avoid 0 tasks and 0 q possibility */
        }

        task.pfn(task.pArgs);
        s->a_nActiveTasks--;

        if (!ThreadPoolBusy(s))
            cnd_signal(&s->cndWait);
    }

    return thrd_success;
}

inline void
ThreadPoolStart(ThreadPool* s)
{
    s->bDone = false;

    for (size_t i = 0; i < s->nThreads; i++)
    {
        [[maybe_unused]] int t = thrd_create(&s->pThreads[i], _ThreadPoolLoop, s);
#ifndef NDEBUG
        fprintf(stderr, "[THREAD POOL]: creating thread '%lu'\n", s->pThreads[i]);
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
        ret = !QueueEmpty(&s->qTasks);
    }

    return ret || s->a_nActiveTasks > 0;
}

inline void
ThreadPoolSubmit(ThreadPool* s, ThreadTask task)
{
    {
        guard::Mtx lock(&s->mtxQ);
        QueuePushBack(&s->qTasks, task);
    }

    cnd_signal(&s->cndQ);
}

inline void
ThreadPoolSubmit(ThreadPool* s, thrd_start_t pfnTask, void* pArgs)
{
    ThreadPoolSubmit(s, {pfnTask, pArgs});
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
        fprintf(stderr, "[THREAD POOL]: trying to stop multiple times or stopping without starting at all\n");
#endif
        return;
    }

    s->bDone = true;
    cnd_broadcast(&s->cndQ);
    for (u32 i = 0; i < s->nThreads; i++)
        thrd_join(s->pThreads[i], nullptr);
}

inline void
ThreadPoolDestroy(ThreadPool* s)
{
    _ThreadPoolStop(s);

    free(s->qTasks.pAlloc, s->pThreads);
    QueueDestroy(&s->qTasks);
    cnd_destroy(&s->cndQ);
    mtx_destroy(&s->mtxQ);
    cnd_destroy(&s->cndWait);
    mtx_destroy(&s->mtxWait);
}

} /* namespace adt */

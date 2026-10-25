#pragma once

#include "atomic.h"

#include <assert.h>

#if defined _WIN32

    #define K_THREAD_WIN32
    #define K_THREAD_LOCAL __declspec( thread )

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #include <windows.h>

    #undef MIN
    #undef MAX
    #undef NEAR
    #undef FAR

    typedef DWORD K_THREAD_RESULT;

#elif defined __unix__

    #define K_THREAD_UNIX
    #define K_THREAD_LOCAL _Thread_local
    #include <pthread.h>
    #include <semaphore.h>
    #include <errno.h>  /* IWYU pragma: keep */
    typedef unsigned K_THREAD_RESULT;

#endif

static const ssize_t K_THREAD_WAIT_INFINITE = 0xffffffff;
static const K_THREAD_RESULT K_THREAD_FAIL = 1;

typedef K_THREAD_RESULT (*k_ThreadFunc)(void* pUser);

static inline void
k_ThreadYield(void)
{
#if defined K_THREAD_WIN32

    SwitchToThread();

#elif defined K_THREAD_UNIX

    sched_yield();

#endif
}

typedef struct
{
    struct
    {
#if defined K_THREAD_WIN32

        HANDLE thread;
        DWORD id;

#elif defined K_THREAD_UNIX

        pthread_t thread;

#endif
    } priv;
} k_Thread;

static inline bool k_ThreadInit(k_Thread* s, k_ThreadFunc pfn, void* pUser);
static inline K_THREAD_RESULT k_ThreadJoin(k_Thread* s);
static inline void k_ThreadDetach(k_Thread* s);

static inline bool
k_ThreadInit(k_Thread* s, k_ThreadFunc pfn, void* pUser)
{
#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

#if defined K_THREAD_WIN32

    s->priv.thread = CreateThread(NULL, 0, pfn, pUser, 0, &s->priv.id);
    assert(s->priv.thread != NULL);

#elif defined K_THREAD_UNIX

    int err = 0;
    err = pthread_create(&s->priv.thread, NULL, (void* (*)(void*))pfn, pUser);
    if (err != 0) return false;

#endif
    return true;

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif
}

static inline K_THREAD_RESULT
k_ThreadJoin(k_Thread* s)
{
#if defined K_THREAD_WIN32

    return WaitForSingleObject(s->priv.thread, K_THREAD_WAIT_INFINITE);

#elif defined K_THREAD_UNIX

    void* ret = 0;
    int err = 0;
    err = pthread_join(s->priv.thread, &ret);
    assert(err == 0);
    (void)err;

    return (K_THREAD_RESULT)(uint64_t)ret;

#endif
}

static inline void
k_ThreadDetach(k_Thread* s)
{
#if defined K_THREAD_WIN32

    (void)s;

#elif defined K_THREAD_UNIX

    int err = pthread_detach(s->priv.thread);
    assert(err == 0);
    (void)err;

#endif
}

typedef enum K_MUTEX_TYPE
{
#if defined K_THREAD_WIN32

    K_MUTEX_TYPE_PLAIN = 0, /* Win32 critical sections are always recursive. */
    K_MUTEX_TYPE_RECURSIVE = 0,

#elif defined K_THREAD_UNIX

    K_MUTEX_TYPE_PLAIN = PTHREAD_MUTEX_NORMAL,
    K_MUTEX_TYPE_RECURSIVE = PTHREAD_MUTEX_RECURSIVE

#endif
} K_MUTEX_TYPE;

typedef struct
{
    struct
    {
#if defined K_THREAD_WIN32

        CRITICAL_SECTION mtx;

#elif defined K_THREAD_UNIX

        pthread_mutex_t mtx;

#endif
    } priv;
} k_Mutex;

static inline bool k_MutexInit(k_Mutex* s, K_MUTEX_TYPE eType);
static inline bool k_MutexInitPlain(k_Mutex* s);
static inline void k_MutexDestroy(k_Mutex* s);
static inline k_Mutex* k_MutexLock(k_Mutex* s); /* Returns s. */
static inline void k_MutexUnlock(k_Mutex* s);
static inline bool k_MutexTryLock(k_Mutex* s);

#define K_MUTEX_LOCK_SCOPE2(s, name)                                                                                   \
    for (int name = (k_MutexLock(s), 0); !name; name = (k_MutexUnlock(s), 1))

// #define K_MUTEX_LOCK_SCOPE(s) K_MUTEX_LOCK_SCOPE2((s), K_GLUE(_kMtxLockScopeVar, __COUNTER__))
#define K_MUTEX_LOCK_SCOPE(s) K_SCOPE_BEGIN_END(k_MutexLock(s), k_MutexUnlock(s))

static inline bool
k_MutexInit(k_Mutex* s, K_MUTEX_TYPE eType)
{
#if defined K_THREAD_WIN32

    (void)eType;
    InitializeCriticalSection(&s->priv.mtx);
    return true;

#elif defined K_THREAD_UNIX

    int err = 0;
    pthread_mutexattr_t attr = {0};
    err = pthread_mutexattr_settype(&attr, eType);
    if (err != 0) return false;

    err = pthread_mutexattr_destroy(&attr);
    assert(err == 0);

    err = pthread_mutex_init(&s->priv.mtx, &attr);
    if (err != 0) return false;

#endif

    return true;
}

static inline bool
k_MutexInitPlain(k_Mutex* s)
{
#if defined K_THREAD_WIN32

    InitializeCriticalSection(&s->priv.mtx);
    return true;

#elif defined K_THREAD_UNIX

    int err = pthread_mutex_init(&s->priv.mtx, NULL);
    assert(err == 0);
    (void)err;
    return true;

#endif
}

static inline void
k_MutexDestroy(k_Mutex* s)
{
#if defined K_THREAD_WIN32

    DeleteCriticalSection(&s->priv.mtx);
    *s = (k_Mutex){0};

#elif defined K_THREAD_UNIX

    int err = pthread_mutex_destroy(&s->priv.mtx);
    assert(err == 0);
    (void)err;

#endif
}

static inline k_Mutex*
k_MutexLock(k_Mutex* s)
{
#if defined K_THREAD_WIN32

    EnterCriticalSection(&s->priv.mtx);

#elif defined K_THREAD_UNIX

    int err = pthread_mutex_lock(&s->priv.mtx);
    assert(err == 0);
    (void)err;

#endif

    return s;
}

static inline void
k_MutexUnlock(k_Mutex* s)
{
#if defined K_THREAD_WIN32

    LeaveCriticalSection(&s->priv.mtx);

#elif defined K_THREAD_UNIX

    int err = pthread_mutex_unlock(&s->priv.mtx);
    assert(err == 0);
    (void)err;

#endif
}

static inline bool
k_MutexTryLock(k_Mutex* s)
{
#if defined K_THREAD_WIN32

    return TryEnterCriticalSection(&s->priv.mtx);

#elif defined K_THREAD_UNIX

    return pthread_mutex_trylock(&s->priv.mtx) != EBUSY;

#endif
}

typedef struct k_CndVar
{
    struct
    {
#if defined K_THREAD_WIN32

        CONDITION_VARIABLE cnd;

#elif defined K_THREAD_UNIX

        pthread_cond_t cnd;

#endif
    } priv;
} k_CndVar;

static inline bool k_CndVarInit(k_CndVar* s);
static inline void k_CndVarDestroy(k_CndVar* s);
static inline void k_CndVarWait(k_CndVar* s, k_Mutex* pMtx);
static inline void k_CndVarSignal(k_CndVar* s);
static inline void k_CndVarBroadcast(k_CndVar* s);

static inline bool
k_CndVarInit(k_CndVar* s)
{
#if defined K_THREAD_WIN32

    InitializeConditionVariable(&s->priv.cnd);
    return true;

#elif defined K_THREAD_UNIX

    /* @MAN: The LinuxThreads implementation supports no attributes for conditions, hence the cond_attr parameter is actually ignored. */
    return pthread_cond_init(&s->priv.cnd, NULL) == 0;

#endif
}

static inline void
k_CndVarDestroy(k_CndVar* s)
{
#if defined K_THREAD_WIN32

    (void)s;

#elif defined K_THREAD_UNIX

    /* @MAN:
     * In the LinuxThreads implementation, no resources are associated with condition variables,
     * thus pthread_cond_destroy actually does nothing except checking that the condition has no waiting threads. */
    int err = pthread_cond_destroy(&s->priv.cnd);
    assert(err == 0);
    (void)err;

#endif
}

static inline void
k_CndVarWait(k_CndVar* s, k_Mutex* pMtx)
{
#if defined K_THREAD_WIN32

    SleepConditionVariableCS(&s->priv.cnd, &pMtx->priv.mtx, K_THREAD_WAIT_INFINITE);

#elif defined K_THREAD_UNIX

    /* @MAN:
     * pthread_cond_wait atomically unlocks the mutex (as per pthread_unlock_mutex) and waits for the condition variable cond to be signaled. 
     * The thread  execution  is  suspended and does not consume any CPU time until the condition variable is signaled.
     * The mutex must be locked by the calling thread on entrance to pthread_cond_wait.
     * Before returning to the calling thread, pthread_cond_wait re-acquires mutex (as per pthread_lock_mutex). */
    int err = pthread_cond_wait(&s->priv.cnd, &pMtx->priv.mtx);
    assert(err == 0);
    (void)err;

#endif
}

static inline void
k_CndVarSignal(k_CndVar* s)
{
#if defined K_THREAD_WIN32

    WakeConditionVariable(&s->priv.cnd);

#elif defined K_THREAD_UNIX

    int err = pthread_cond_signal(&s->priv.cnd);
    (void)err;
    assert(err == 0);

#endif
}

static inline void
k_CndVarBroadcast(k_CndVar* s)
{
#if defined K_THREAD_WIN32

    WakeAllConditionVariable(&s->priv.cnd);

#elif defined K_THREAD_UNIX

    int err = pthread_cond_broadcast(&s->priv.cnd);
    assert(err == 0);
    (void)err;

#endif
}

typedef struct k_Semaphore
{
    struct
    {
#if defined K_THREAD_WIN32

        HANDLE sem;

#elif defined K_THREAD_UNIX

        sem_t sem;

#endif
    } priv;
} k_Semaphore;

static inline bool k_SemaphoreInit(k_Semaphore* s, int initialCount);
static inline void k_SemaphoreDestroy(k_Semaphore* s);
static inline void k_SemaphoreInc(k_Semaphore* s);
static inline void k_SemaphoreDec(k_Semaphore* s);

static inline bool
k_SemaphoreInit(k_Semaphore* s, int initialCount)
{
#if defined K_THREAD_WIN32

    s->priv.sem = CreateSemaphoreA(NULL, initialCount, INT16_MAX, NULL);
    if (s->priv.sem == NULL) return false;

#elif defined K_THREAD_UNIX

    if (sem_init(&s->priv.sem, 0, initialCount) != 0)
        return false;

#endif

    return true;
}

static inline void
k_SemaphoreDestroy(k_Semaphore* s)
{
#if defined K_THREAD_WIN32

    bool b = CloseHandle(s->priv.sem);
    (void)b;
    assert(b);

#elif defined K_THREAD_UNIX

    sem_destroy(&s->priv.sem);

#endif
}

static inline void
k_SemaphoreInc(k_Semaphore* s)
{
#if defined K_THREAD_WIN32

    bool b = ReleaseSemaphore(s->priv.sem, 1, NULL);
    (void)b;
    assert(b);

#elif defined K_THREAD_UNIX

    sem_post(&s->priv.sem);

#endif
}

static inline void
k_SemaphoreIncN(k_Semaphore* s, int n)
{
#if defined K_THREAD_WIN32

    bool b = ReleaseSemaphore(s->priv.sem, n, NULL);
    (void)b;
    assert(b);

#elif defined K_THREAD_UNIX

    for (int i = 0; i < n; ++i)
        sem_post(&s->priv.sem);

#endif
}

static inline void
k_SemaphoreDec(k_Semaphore* s)
{
#if defined K_THREAD_WIN32

    WaitForSingleObject(s->priv.sem, K_THREAD_WAIT_INFINITE);

#elif defined K_THREAD_UNIX

    sem_wait(&s->priv.sem);

#endif
}

typedef struct k_TicketMutex
{
    k_atomic_U64 ticketId;
    char aPad[64];
    k_atomic_U64 servingId;
} k_TicketMutex;

static inline k_TicketMutex k_TicketMutexCreate(void);
static inline void k_TicketMutexLock(k_TicketMutex* s);
static inline void k_TicketMutexUnlock(k_TicketMutex* s);

static inline k_TicketMutex
k_TicketMutexCreate(void)
{
    return (k_TicketMutex){0};
}

static inline void
k_TicketMutexLock(k_TicketMutex* s)
{
    uint64_t ticket = k_atomic_U64AddRelaxed(&s->ticketId, 1);
    while (ticket != k_atomic_U64LoadRelaxed(&s->servingId))
        ;
}

static inline void
k_TicketMutexUnlock(k_TicketMutex* s)
{
    k_atomic_U64AddRelaxed(&s->servingId, 1);
}

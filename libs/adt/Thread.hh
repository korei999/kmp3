#pragma once

#include "adt/types.hh"

#if __has_include(<windows.h>)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #define ADT_USE_WIN32THREAD
#elif __has_include(<pthread.h>)
    #include <pthread.h>
    #define ADT_USE_PTHREAD
#endif

namespace adt
{

constexpr ssize THREAD_WAIT_INFINITE = 0xffffffff;

using THREAD_STATUS = usize;
using ThreadFn = THREAD_STATUS (*)(void*);

struct Thread
{
#ifdef ADT_USE_PTHREAD

    pthread_t m_thread {};

#elif defined ADT_USE_WIN32THREAD

    HANDLE m_thread {};
    DWORD m_id {};

#else
    #error "No platform threads"
#endif

    /* */

    Thread() = default;
    Thread(THREAD_STATUS (*pfn)(void*), void* pFnArg);

    /* */

    THREAD_STATUS join();
    THREAD_STATUS detach();

private:
#ifdef ADT_USE_PTHREAD

    THREAD_STATUS pthreadJoin();
    THREAD_STATUS pthreadDetach();

#elif defined ADT_USE_WIN32THREAD

    THREAD_STATUS win32Join();
    THREAD_STATUS win32Detach();

#endif
};

inline
Thread::Thread(THREAD_STATUS (*pfn)(void*), void* pFnArg)
{
#ifdef ADT_USE_PTHREAD

    pthread_create(&m_thread, {}, (void* (*)(void*))pfn, pFnArg);

#elif defined ADT_USE_WIN32THREAD

    m_thread = CreateThread(nullptr, 0, (DWORD (*)(void*))pfn, pFnArg, 0, &m_id);

#endif
}

inline THREAD_STATUS
Thread::join()
{
#ifdef ADT_USE_PTHREAD
    return pthreadJoin();
#elif defined ADT_USE_WIN32THREAD
    return win32Join();
#endif
}

inline THREAD_STATUS
Thread::detach()
{
#ifdef ADT_USE_PTHREAD
    return pthreadDetach();
#elif defined ADT_USE_WIN32THREAD
    return win32Detach();
#endif
}


#ifdef ADT_USE_PTHREAD

inline THREAD_STATUS
Thread::pthreadJoin()
{
    void* pRet {};
    pthread_join(m_thread, &pRet);

    return (THREAD_STATUS)pRet;
}

inline THREAD_STATUS
Thread::pthreadDetach()
{
    return (THREAD_STATUS)pthread_detach(m_thread);
}

#elif defined ADT_USE_WIN32THREAD

inline THREAD_STATUS
Thread::win32Join()
{
    return WaitForSingleObject(m_thread, THREAD_WAIT_INFINITE);
}

inline THREAD_STATUS
Thread::win32Detach()
{
    return {};
}

#endif

enum MUTEX_TYPE : u8
{
    PLAIN = 0,
    RECURSIVE = 1,
};

struct Mutex
{
#ifdef ADT_USE_PTHREAD

    pthread_mutex_t m_mtx {};
    pthread_mutexattr_t m_attr {};

#elif defined ADT_USE_WIN32THREAD

    CRITICAL_SECTION m_mtx {};

#endif

    /* */
    
    Mutex() = default;
    Mutex(MUTEX_TYPE eType);

    /* */

    void lock();
    bool tryLock();
    void unlock();
    void destroy();

private:
#ifdef ADT_USE_PTHREAD

    int pthreadAttrType(MUTEX_TYPE eType) { return (int)eType; }

#elif defined ADT_USE_WIN32THREAD

#endif
};

inline
Mutex::Mutex(MUTEX_TYPE eType)
{
#ifdef ADT_USE_PTHREAD

    pthread_mutexattr_init(&m_attr);
    pthread_mutexattr_settype(&m_attr, pthreadAttrType(eType));
    pthread_mutex_init(&m_mtx, &m_attr);

#elif defined ADT_USE_WIN32THREAD

    InitializeCriticalSection(&m_mtx);

#endif
}

inline void
Mutex::lock()
{
#ifdef ADT_USE_PTHREAD

    pthread_mutex_lock(&m_mtx);

#elif defined ADT_USE_WIN32THREAD

    EnterCriticalSection(&m_mtx);

#endif
}

inline bool
Mutex::tryLock()
{
#ifdef ADT_USE_PTHREAD

    return pthread_mutex_trylock(&m_mtx);

#elif defined ADT_USE_WIN32THREAD

    return TryEnterCriticalSection(&m_mtx);

#endif
}

inline void
Mutex::unlock()
{
#ifdef ADT_USE_PTHREAD

    pthread_mutex_unlock(&m_mtx);

#elif defined ADT_USE_WIN32THREAD

    LeaveCriticalSection(&m_mtx);

#endif
}

inline void
Mutex::destroy()
{
#ifdef ADT_USE_PTHREAD

    pthread_mutex_destroy(&m_mtx);
    pthread_mutexattr_destroy(&m_attr);
    *this = {};

#elif defined ADT_USE_WIN32THREAD

    DeleteCriticalSection(&m_mtx);
    *this = {};

#endif
}

struct CndVar
{
#ifdef ADT_USE_PTHREAD

    pthread_cond_t m_cnd {};

#elif defined ADT_USE_WIN32THREAD

    CONDITION_VARIABLE m_cnd {};

#endif

    /* */

    CndVar() = default;
    CndVar(INIT_FLAG);

    /* */

    void destroy();
    void wait(Mutex* pMtx);
    void timedWait(Mutex* pMtx, f64 ms);
    void signal();
    void broadcast();
};

inline
CndVar::CndVar(INIT_FLAG)
{
#ifdef ADT_USE_PTHREAD

    pthread_cond_init(&m_cnd, {});

#elif defined ADT_USE_WIN32THREAD

    InitializeConditionVariable(&m_cnd);

#endif
}

inline void
CndVar::destroy()
{
#ifdef ADT_USE_PTHREAD

    pthread_cond_destroy(&m_cnd);
    *this = {};

#elif defined ADT_USE_WIN32THREAD

    *this = {};
#endif
}

inline void
CndVar::wait(Mutex* pMtx)
{
#ifdef ADT_USE_PTHREAD

    pthread_cond_wait(&m_cnd, &pMtx->m_mtx);

#elif defined ADT_USE_WIN32THREAD

    SleepConditionVariableCS(&m_cnd, &pMtx->m_mtx, THREAD_WAIT_INFINITE);

#endif
}

inline void
CndVar::timedWait(Mutex* pMtx, f64 ms)
{
#ifdef ADT_USE_PTHREAD

    timespec ts {
        .tv_sec = ssize(ms / 1000.0),
        .tv_nsec = (ssize(ms) % 1000) * 1000'000,
    };
    pthread_cond_timedwait(&m_cnd, &pMtx->m_mtx, &ts);

#elif defined ADT_USE_WIN32THREAD

    SleepConditionVariableCS(&m_cnd, &pMtx->m_mtx, ssize(ms));

#endif
}

inline void
CndVar::signal()
{
#ifdef ADT_USE_PTHREAD

    pthread_cond_signal(&m_cnd);

#elif defined ADT_USE_WIN32THREAD

    WakeConditionVariable(&m_cnd);

#endif
}

inline void
CndVar::broadcast()
{
#ifdef ADT_USE_PTHREAD

    pthread_cond_broadcast(&m_cnd);

#elif defined ADT_USE_WIN32THREAD

    WakeAllConditionVariable(&m_cnd);

#endif
}

} /* namespace adt */

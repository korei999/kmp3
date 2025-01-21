#pragma once

#if __has_include(<pthread.h>)
    #include <pthread.h>
    #define ADT_USE_PTHREAD
#elif __has_include(<windows.h>)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #define ADT_USE_WIN32THREAD
#endif

namespace adt
{

struct Thread
{
#ifdef ADT_USE_PTHREAD
    pthread_t m_thread {};
#else
    #error "No platform threads"
#endif

    Thread() = default;
    Thread(void* (*pfn)(void*), void* pFnArg);

    /* */

    void* join();
    int detach();
};

inline
Thread::Thread(void* (*pfn)(void*), void* pFnArg)
{
    pthread_create(&m_thread, {}, pfn, pFnArg);
}

inline void*
Thread::join()
{
    void* pRet {};
    pthread_join(m_thread, &pRet);

    return pRet;
}

inline int
Thread::detach()
{
    return pthread_detach(m_thread);
}

} /* namespace adt */

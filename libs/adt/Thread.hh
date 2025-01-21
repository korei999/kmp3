#pragma once

#include <threads.h>

namespace adt
{

/* Exit and error codes.  */
enum THREAD_STATUS : int
{
    SUCCESS = 0,
    BUSY = 1,
    ERROR = 2,
    NOMEM = 3,
    TIMEDOUT = 4
};

struct Thread
{
    thrd_t m_thread {};
    int m_id {};

    /* */

    Thread() = default;
    Thread(int (*pfn)(void*), void* pFnArg);

    /* */

    THREAD_STATUS join();
    THREAD_STATUS detach();
};

inline
Thread::Thread(int (*pfn)(void*), void* pFnArg)
{
    m_id = thrd_create(&m_thread, pfn, pFnArg);
}

inline THREAD_STATUS
Thread::join()
{
    int status {};
    int err = thrd_join(m_thread, &status);

    return (THREAD_STATUS)status;
}

inline THREAD_STATUS
Thread::detach()
{
    int err = thrd_detach(m_thread);
    return (THREAD_STATUS)err;
}

} /* namespace adt */

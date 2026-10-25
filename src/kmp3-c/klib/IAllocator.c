#include "IAllocator.h"

#if defined __unix__

    #include <unistd.h>

#elif defined _WIN32

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #include <windows.h>

#endif

static ssize_t s_pageSize;

ssize_t
k_getPageSize(void)
{
#if defined __unix__
    if (s_pageSize == 0) s_pageSize = getpagesize();
#elif defined _WIN32
    if (s_pageSize == 0)
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        s_pageSize = sysInfo.dwPageSize;
    }
#else
    if (s_pageSize == 0) s_pageSize = K_SIZE_1K * 4;
#endif

    return s_pageSize;
}

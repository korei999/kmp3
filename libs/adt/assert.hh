#pragma once

#include "types.hh"

#include <cstdio>
#include <cstdlib>

namespace adt
{

inline void
assertionFailed(const char* cnd, const char* msg, const char* file, int line, const char* func)
{
    char aBuff[256] {};
    snprintf(aBuff, sizeof(aBuff) - 1, "[%s, %d: %s()] assertion( %s ) failed.\n(msg) '%s'\n", file, line, func, cnd, msg);

#if __has_include(<windows.h>)
    MessageBoxA(
        nullptr,
        aBuff,
        "Assertion failed",
        MB_ICONWARNING | MB_OK | MB_DEFBUTTON2
    );
#else
    fputs(aBuff, stderr);
    fflush(stderr);
#endif

#ifndef NDEBUG
    abort();
#endif
}

} /* namespace adt */

#ifndef NDEBUG
    #define ADT_ASSERT(CND, ...)                                                                                       \
        /* this has to be formatted with standard printf because of mutual inclusion problem */                        \
        do                                                                                                             \
        {                                                                                                              \
            if (!static_cast<bool>(CND))                                                                               \
            {                                                                                                          \
                char aMsgBuff[128] {};                                                                                 \
                snprintf(aMsgBuff, sizeof(aMsgBuff) - 1, __VA_ARGS__);                                                 \
                adt::assertionFailed(#CND, aMsgBuff, ADT_LOGS_FILE, __LINE__, __func__);                               \
            }                                                                                                          \
        } while (0)
#else
    #define ADT_ASSERT(...) (void)0
#endif

#ifndef ADT_DISABLE_ASSERT_ALWAYS
    #define ADT_ASSERT_ALWAYS(CND, ...)                                                                                \
        do                                                                                                             \
        {                                                                                                              \
            if (!static_cast<bool>(CND))                                                                               \
            {                                                                                                          \
                char aMsgBuff[128] {};                                                                                 \
                snprintf(aMsgBuff, sizeof(aMsgBuff) - 1, __VA_ARGS__);                                                 \
                adt::assertionFailed(#CND, aMsgBuff, ADT_LOGS_FILE, __LINE__, __func__);                               \
            }                                                                                                          \
        } while (0)
#else
    #define ADT_ASSERT_ALWAYS(...) (void)0
#endif

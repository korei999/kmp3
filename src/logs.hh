#pragma once

#include "adt/print.hh"

#include <cassert>
#include <cstdlib>

#ifndef LOGS_NO_COLORS
    #define COL_NORM "\x1B[0m"
    #define COL_RED "\x1B[31m"
    #define COL_GREEN "\x1B[32m"
    #define COL_YELLOW "\x1B[33m"
    #define COL_BLUE "\x1B[34m"
    #define COL_MAGENTA "\x1B[35m"
    #define COL_CYAN "\x1B[36m"
    #define COL_WHITE "\x1B[37m"
#else
    #define COL_NORM ""
    #define COL_RED ""
    #define COL_GREEN ""
    #define COL_YELLOW ""
    #define COL_BLUE ""
    #define COL_MAGENTA ""
    #define COL_CYAN ""
    #define COL_WHITE ""
#endif

#define COUT(...) adt::print::cout(__VA_ARGS__)
#define CERR(...) adt::print::cerr(__VA_ARGS__)

#ifndef NDEBUG
    #define DCOUT(...) COUT(__VA_ARGS__)
    #define DCERR(...) CERR(__VA_ARGS__)
#else
    #define DCOUT(...) (void)0
    #define DCERR(...) (void)0
#endif

enum _LOG_SEV
{
    _LOG_SEV_OK,
    _LOG_SEV_GOOD,
    _LOG_SEV_NOTIFY,
    _LOG_SEV_WARN,
    _LOG_SEV_BAD,
    _LOG_SEV_FATAL,
    _LOG_SEV_ENUM_SIZE
};

inline const char* _LOG_SEV_STR[] = {
    "",
    COL_GREEN "GOOD: " COL_NORM,
    COL_CYAN "NOTIFY: " COL_NORM,
    COL_YELLOW "WARNING: " COL_NORM,
    COL_RED "BAD: " COL_NORM,
    COL_RED "FATAL: " COL_NORM
};

#if defined __clang__ || __GNUC__
    #define LOGS_FILE __FILE_NAME__
#else
    #define LOGS_FILE __FILE__
#endif

#ifdef LOGS
    #define _LOG(SEV, ...)                                                                                             \
        do                                                                                                             \
        {                                                                                                              \
            assert(SEV >= 0 && SEV < _LOG_SEV_ENUM_SIZE && "wrong _LOG_SEV*");                                         \
            CERR("({}{}, {}): ", _LOG_SEV_STR[SEV], LOGS_FILE, __LINE__);                                              \
            CERR(__VA_ARGS__);                                                                                         \
            switch (SEV)                                                                                               \
            {                                                                                                          \
                default:                                                                                               \
                    break;                                                                                             \
                case _LOG_SEV_BAD:                                                                                     \
                    exit(int(SEV));                                                                                    \
                case _LOG_SEV_FATAL:                                                                                   \
                    abort();                                                                                           \
            }                                                                                                          \
        } while (0)

    #define LOG(...) _LOG(_LOG_SEV_OK, __VA_ARGS__)
    #define LOG_OK(...) _LOG(_LOG_SEV_OK, __VA_ARGS__)
    #define LOG_GOOD(...) _LOG(_LOG_SEV_GOOD, __VA_ARGS__)
    #define LOG_NOTIFY(...) _LOG(_LOG_SEV_NOTIFY, __VA_ARGS__)
    #define LOG_WARN(...) _LOG(_LOG_SEV_WARN, __VA_ARGS__)
    #define LOG_BAD(...) _LOG(_LOG_SEV_BAD, __VA_ARGS__)
    #define LOG_FATAL(...) _LOG(_LOG_SEV_FATAL, __VA_ARGS__)
#else
    #define _LOG (void)0
    #define LOG(...) (void)0
    #define LOG_OK(...) (void)0
    #define LOG_GOOD(...) (void)0
    #define LOG_NOTIFY(...) (void)0
    #define LOG_WARN(...) (void)0
    #define LOG_BAD(...) (void)0
    #define LOG_FATAL(...) (void)0
#endif

/* TODO: */
// #ifdef _WIN32
//     #define ASSERT assert
// #else
//     #define ASSERT(EXPR, ...)
// #endif

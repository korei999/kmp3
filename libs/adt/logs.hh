#pragma once

#include "print.hh"

#include <cassert>
#include <cstdlib>

#define ADT_LOGS_COL_NORM  "\x1B[0m"
#define ADT_LOGS_COL_RED  "\x1B[31m"
#define ADT_LOGS_COL_GREEN  "\x1B[32m"
#define ADT_LOGS_COL_YELLOW  "\x1B[33m"
#define ADT_LOGS_COL_BLUE  "\x1B[34m"
#define ADT_LOGS_COL_MAGENTA  "\x1B[35m"
#define ADT_LOGS_COL_CYAN  "\x1B[36m"
#define ADT_LOGS_COL_WHITE  "\x1B[37m"

#define ADT_COUT(...) adt::print::out(__VA_ARGS__)
#define ADT_CERR(...) adt::print::err(__VA_ARGS__)

#ifndef NDEBUG
    #define ADT_DCOUT(...) ADT_COUT(__VA_ARGS__)
    #define ADT_DCERR(...) ADT_CERR(__VA_ARGS__)
#else
    #define ADT_DCOUT(...) (void)0
    #define ADT_DCERR(...) (void)0
#endif

enum _ADT_LOG_SEV
{
    _ADT_LOG_SEV_OK,
    _ADT_LOG_SEV_GOOD,
    _ADT_LOG_SEV_NOTIFY,
    _ADT_LOG_SEV_WARN,
    _ADT_LOG_SEV_BAD,
    _ADT_LOG_SEV_EXIT,
    _ADT_LOG_SEV_FATAL,
    _ADT_LOG_SEV_ENUM_SIZE
};

constexpr adt::String _ADT_LOG_SEV_STR[] = {
    "",
    ADT_LOGS_COL_GREEN "GOOD: " ADT_LOGS_COL_NORM,
    ADT_LOGS_COL_CYAN "NOTIFY: " ADT_LOGS_COL_NORM,
    ADT_LOGS_COL_YELLOW "WARNING: " ADT_LOGS_COL_NORM,
    ADT_LOGS_COL_RED "BAD: " ADT_LOGS_COL_NORM,
    ADT_LOGS_COL_MAGENTA "EXIT: " ADT_LOGS_COL_NORM,
    ADT_LOGS_COL_RED "FATAL: " ADT_LOGS_COL_NORM
};

#if defined __clang__ || __GNUC__
    #define ADT_LOGS_FILE __FILE_NAME__
#else
    #define ADT_LOGS_FILE __FILE__
#endif

#ifdef ADT_LOGS
    #define _ADT_LOG(SEV, ...)                                                                                         \
        do                                                                                                             \
        {                                                                                                              \
            assert(SEV >= 0 && SEV < _ADT_LOG_SEV_ENUM_SIZE && "wrong _ADT_LOG_SEV*");                                 \
            CERR("({}{}, {}): ", _ADT_LOG_SEV_STR[SEV], ADT_LOGS_FILE, __LINE__);                                      \
            CERR(__VA_ARGS__);                                                                                         \
            switch (SEV)                                                                                               \
            {                                                                                                          \
                default:                                                                                               \
                    break;                                                                                             \
                case _ADT_LOG_SEV_EXIT:                                                                                \
                    exit(EXIT_FAILURE);                                                                                \
                case _ADT_LOG_SEV_FATAL:                                                                               \
                    abort();                                                                                           \
            }                                                                                                          \
        } while (0)
#else
    #define _ADT_LOG(SEV, ...) (void)0
#endif

#define ADT_LOG(...) _ADT_LOG(_ADT_LOG_SEV_OK, __VA_ARGS__)
#define ADT_LOG_OK(...) _ADT_LOG(_ADT_LOG_SEV_OK, __VA_ARGS__)
#define ADT_LOG_GOOD(...) _ADT_LOG(_ADT_LOG_SEV_GOOD, __VA_ARGS__)
#define ADT_LOG_NOTIFY(...) _ADT_LOG(_ADT_LOG_SEV_NOTIFY, __VA_ARGS__)
#define ADT_LOG_WARN(...) _ADT_LOG(_ADT_LOG_SEV_WARN, __VA_ARGS__)
#define ADT_LOG_BAD(...) _ADT_LOG(_ADT_LOG_SEV_BAD, __VA_ARGS__)
#define ADT_LOG_EXIT(...) _ADT_LOG(_ADT_LOG_SEV_EXIT, __VA_ARGS__)
#define ADT_LOG_FATAL(...) _ADT_LOG(_ADT_LOG_SEV_FATAL, __VA_ARGS__)

#ifdef ADT_LOGS_LESS_TYPING
    #define _LOG(...) _ADT_LOG(__VA_ARGS__)
    #define LOG(...) ADT_LOG(__VA_ARGS__)
    #define LOG_OK(...) ADT_LOG_OK(__VA_ARGS__)
    #define LOG_GOOD(...) ADT_LOG_GOOD(__VA_ARGS__)
    #define LOG_NOTIFY(...) ADT_LOG_NOTIFY(__VA_ARGS__)
    #define LOG_WARN(...) ADT_LOG_WARN(__VA_ARGS__)
    #define LOG_BAD(...) ADT_LOG_BAD(__VA_ARGS__)
    #define LOG_EXIT(...) ADT_LOG_EXIT(__VA_ARGS__)
    #define LOG_FATAL(...) ADT_LOG_FATAL(__VA_ARGS__)

    #define COUT(...) ADT_COUT(__VA_ARGS__)
    #define CERR(...) ADT_CERR(__VA_ARGS__)

    #define DCOUT(...) ADT_DCOUT(__VA_ARGS__)
    #define DCERR(...) ADT_DCERR(__VA_ARGS__)
#endif

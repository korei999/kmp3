#pragma once

#include "Arena.h"
#include "Thread.h"
#include "RingMPSC.h"
#include "StringView.h"

#include <stdio.h>

#define K_LOGGER_ANSI_COLOR_NORM  "\x1b[0m"
#define K_LOGGER_ANSI_COLOR_BOLD "\x1b[1m"
#define K_LOGGER_ANSI_COLOR_DIM "\x1b[2m"
#define K_LOGGER_ANSI_COLOR_ITALIC "\x1b[3m"
#define K_LOGGER_ANSI_COLOR_UNDERLINE "\x1b[4m"
#define K_LOGGER_ANSI_COLOR_BLINK "\x1b[5m"
#define K_LOGGER_ANSI_COLOR_REVERSE "\x1b[7m"
#define K_LOGGER_ANSI_COLOR_INVIS "\x1b[8m"
#define K_LOGGER_ANSI_COLOR_STRIKE "\x1b[9m"

#define K_LOGGER_ANSI_COLOR_RED  "\x1b[31m"
#define K_LOGGER_ANSI_COLOR_GREEN  "\x1b[32m"
#define K_LOGGER_ANSI_COLOR_YELLOW  "\x1b[33m"
#define K_LOGGER_ANSI_COLOR_BLUE  "\x1b[34m"
#define K_LOGGER_ANSI_COLOR_MAGENTA  "\x1b[35m"
#define K_LOGGER_ANSI_COLOR_CYAN  "\x1b[36m"
#define K_LOGGER_ANSI_COLOR_WHITE  "\x1b[37m"

#define K_LOGGER_ANSI_COLOR_BG_RED "\x1b[41m"
#define K_LOGGER_ANSI_COLOR_BG_GREEN "\x1b[42m"
#define K_LOGGER_ANSI_COLOR_BG_YELLOW "\x1b[43m"
#define K_LOGGER_ANSI_COLOR_BG_BLUE "\x1b[44m"
#define K_LOGGER_ANSI_COLOR_BG_MAGENTA "\x1b[45m"
#define K_LOGGER_ANSI_COLOR_BG_CYAN "\x1b[46m"
#define K_LOGGER_ANSI_COLOR_BG_WHITE "\x1b[47m"

typedef uint8_t K_LOGGER_LEVEL;
static const K_LOGGER_LEVEL K_LOGGER_LEVEL_NONE = 0;
static const K_LOGGER_LEVEL K_LOGGER_LEVEL_WARNING = 1;
static const K_LOGGER_LEVEL K_LOGGER_LEVEL_ERROR = 2;
static const K_LOGGER_LEVEL K_LOGGER_LEVEL_INFO = 3;
static const K_LOGGER_LEVEL K_LOGGER_LEVEL_DEBUG = 4;

typedef uint8_t K_LOGGER_FLAGS;
static const K_LOGGER_FLAGS K_LOGGER_FLAG_COLORS = 1;
static const K_LOGGER_FLAGS K_LOGGER_FLAG_TIME = 1 << 1;
static const K_LOGGER_FLAGS K_LOGGER_FLAG_SOURCE = 1 << 2;
static const K_LOGGER_FLAGS K_LOGGER_FLAG_FUNC = 1 << 3;

struct k_Logger;

typedef ssize_t (*k_LoggerFormatHeaderPfn)(struct k_Logger* s, void* pArg, K_LOGGER_LEVEL eLogLevel, const char* ntsFile, const char* ntsFunc, ssize_t line, k_Span sp);
typedef ssize_t (*k_LoggerSinkPfn)(struct k_Logger* s, void* pArg, k_Span sp);

typedef struct k_Logger
{
    k_IAllocator* pAlloc;
    k_LoggerFormatHeaderPfn pfnFormatHeader;
    void* pFormatHeaderArg;
    k_LoggerSinkPfn pfnSink;
    void* pSinkArg;
    k_RingMPSC rb;
    k_atomic_Int nPosts;
    ssize_t buffSize;
    uint8_t* pDrainBuff;
    uint8_t* pSecondaryBuff;
    k_Semaphore sem;
    k_Thread thread;
    int fd;
    K_LOGGER_LEVEL eLogLevel;
    bool bStarted;
    K_LOGGER_FLAGS eFlags;
    k_atomic_U8 bDone;
} k_Logger;

typedef struct k_LoggerInitOpts
{
    k_LoggerFormatHeaderPfn pfnFormat; /* k_LoggerDefaultFormatter if null. */
    void* pFormatArg;
    k_LoggerSinkPfn pfnSink; /* k_LoggerDefaultSink if null. */
    void* pSinkArg;
    ssize_t ringBufferSize; /* Do not init if 0. */
    int fd; /* 2 (stderr) if 0. */
    K_LOGGER_LEVEL eLogLevel;
    K_LOGGER_FLAGS eFlags;
} k_LoggerInitOpts;

bool k_LoggerInit(k_Logger* s, k_IAllocator* pAlloc, k_LoggerInitOpts opts);
void k_LoggerDestroy(k_Logger* s);
void k_LoggerPostVaList(k_Logger* s, k_Arena* pArena, K_LOGGER_LEVEL eLevel, const char* ntsFile, const char* ntsFunc, ssize_t line, const k_StringView svFmt, va_list* pArgs);
void k_LoggerPostSv(k_Logger* s, k_Arena* pArena, K_LOGGER_LEVEL eLevel, const char* ntsFile, const char* ntsFunc, ssize_t line, const k_StringView svFmt, ...);
void k_LoggerPost(k_Logger* s, k_Arena* pArena, K_LOGGER_LEVEL eLevel, const char* ntsFile, const char* ntsFunc, ssize_t line, const char* ntsFmt, ...);
ssize_t k_LoggerDefaultFormatter(k_Logger* s, void* pArg, K_LOGGER_LEVEL eLevel, const char* ntsFile, const char* ntsFunc, ssize_t line, k_Span spSink);
ssize_t k_LoggerDefaultSink(k_Logger* s, void* pArg, k_Span sp);

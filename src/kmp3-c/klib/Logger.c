#include "Logger.h"

#include "file.h"
#include "print.h"

#ifdef _WIN32
    #include <io.h>
#endif

#include <time.h>

typedef struct LogHeader
{
    const char* ntsFile;
    const char* ntsFunc;
    size_t lineAndLevel; /* Most significant (leftmost) byte is log level. */
    uint8_t pMem[];
} LogHeader;

static K_THREAD_RESULT
k_LoggerLoop(void* pArg)
{
    k_Logger* s = pArg;

    while (true)
    {
        ssize_t line = 0;
        K_LOGGER_LEVEL eLevel = 0;
        LogHeader* pHeader = (LogHeader*)s->pDrainBuff;

        const int nPosts = k_atomic_IntLoadRelaxed(&s->nPosts);

        if (nPosts > 0)
        {
            k_Span sp = k_RingMPSCPop(&s->rb, (k_RingMPSCPopOpts){
                .pDestOrNull = pHeader,
                .destSize = s->buffSize}
            );
            if (sp.pData == NULL) continue;

            eLevel = pHeader->lineAndLevel >> 56;
            line = pHeader->lineAndLevel & ~(255ull << 56ull);

            const ssize_t nn = s->pfnFormatHeader(
                s, s->pFormatHeaderArg, eLevel, pHeader->ntsFile, pHeader->ntsFunc, line,
                (k_Span){.pData = s->pSecondaryBuff, .size = s->buffSize}
            );
            memcpy(s->pSecondaryBuff + nn, (uint8_t*)sp.pData + sizeof(LogHeader), sp.size - sizeof(LogHeader));
            s->pfnSink(s, s->pSinkArg, (k_Span){s->pSecondaryBuff, nn + sp.size - sizeof(LogHeader)});

            k_atomic_IntSubRelaxed(&s->nPosts, 1);
        }
        else
        {
            if (k_atomic_U8LoadRelaxed(&s->bDone) && nPosts <= 0) break;
            k_SemaphoreDec(&s->sem);
        }
    }

    return 0;
}

bool
k_LoggerInit(k_Logger* s, k_IAllocator* pAlloc, k_LoggerInitOpts opts)
{
    if (opts.ringBufferSize <= 0) return true;

    s->sem = (k_Semaphore){0};
    if (!k_SemaphoreInit(&s->sem, 1)) return false;

    s->pAlloc = pAlloc;

    if (!k_RingMPSCInit(&s->rb, pAlloc, opts.ringBufferSize)) return false;

    s->buffSize = s->rb.capMinus1 + 1;

    s->pDrainBuff = k_IAllocatorZalloc(pAlloc, s->buffSize * 2);
    if (!s->pDrainBuff)
    {
        k_RingMPSCDestroy(&s->rb, pAlloc);
        return false;
    }
    s->pSecondaryBuff = s->pDrainBuff + s->buffSize;

    s->nPosts.volNum = 0;

    if (opts.pfnFormat) s->pfnFormatHeader = opts.pfnFormat;
    else s->pfnFormatHeader = k_LoggerDefaultFormatter;

    if (opts.pfnSink) s->pfnSink = opts.pfnSink;
    else s->pfnSink = k_LoggerDefaultSink;

    if (opts.fd) s->fd = opts.fd;
    else s->fd = 2;

    s->eLogLevel = opts.eLogLevel;
    s->bDone.volNum = false;

    s->eFlags = opts.eFlags;

    if ((opts.eFlags & K_LOGGER_FLAG_COLORS) || k_file_isatty(opts.fd))
        s->eFlags |= K_LOGGER_FLAG_COLORS;
    else s->eFlags &= ~K_LOGGER_FLAG_COLORS;

    k_ThreadInit(&s->thread, k_LoggerLoop, s);

    s->bStarted = true;

    return true;
}

void
k_LoggerDestroy(k_Logger* s)
{
    if (!s->bStarted) return;

    k_atomic_U8StoreRelaxed(&s->bDone, true);
    k_SemaphoreInc(&s->sem);

    k_ThreadJoin(&s->thread);

    k_SemaphoreDestroy(&s->sem);
    k_RingMPSCDestroy(&s->rb, s->pAlloc);
    k_IAllocatorFree(s->pAlloc, s->pDrainBuff);
}

static bool
k_LoggerPushMsg(k_Logger* s, K_LOGGER_LEVEL eLevel, const char* ntsFile, const char* ntsFunc, ssize_t line, const k_StringView svMsg)
{
    if (svMsg.size + (ssize_t)sizeof(LogHeader) + k_RingMPSCHeaderSize() > k_RingMPSCCap(&s->rb)) return false;

    LogHeader lh = {
        .ntsFile = ntsFile,
        .ntsFunc = ntsFunc,
        .lineAndLevel = (size_t)line | ((size_t)eLevel << 56ull),
    };

    k_Span aSps[] = {
        {&lh, sizeof(lh)},
        {svMsg.pData, svMsg.size},
    };

    while (true)
    {
        if (k_atomic_U8LoadRelaxed(&s->bDone)) return false;

        if (k_RingMPSCPushV(&s->rb, aSps, K_ASIZE(aSps)))
            break;

        k_ThreadYield();
    }

    k_atomic_IntAddRelaxed(&s->nPosts, 1);
    k_SemaphoreInc(&s->sem);
    return true;
}

void
k_LoggerPostVaList(k_Logger* s, k_Arena* pArena, K_LOGGER_LEVEL eLevel, const char* ntsFile, const char* ntsFunc, ssize_t line, const k_StringView svFmt, va_list* pArgs)
{
    if (eLevel > s->eLogLevel) return;

    K_ARENA_SCOPE(pArena)
    {
        k_print_Builder pb;
        if (k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &pArena->base, .preallocOrBufferSize = 256}))
        {
            k_print_FmtArgs fmtArgs = k_print_FmtArgsCreate();
            k_print_BuilderPrintVaList(&pb, &fmtArgs, svFmt, pArgs);
            k_print_BuilderPushChar(&pb, '\n');
            k_LoggerPushMsg(s, eLevel, ntsFile, ntsFunc, line, k_print_BuilderToSv(&pb));
        }
    }
}

void
k_LoggerPostSv(k_Logger* s, k_Arena* pArena, K_LOGGER_LEVEL eLevel, const char* ntsFile, const char* ntsFunc, ssize_t line, const k_StringView svFmt, ...)
{
    if (eLevel > s->eLogLevel) return;

    va_list args;
    va_start(args, svFmt);
    k_LoggerPostVaList(s, pArena, eLevel, ntsFile, ntsFunc, line, svFmt, &args);
    va_end(args);
}

void
k_LoggerPost(k_Logger* s, k_Arena* pArena, K_LOGGER_LEVEL eLevel, const char* ntsFile, const char* ntsFunc, ssize_t line, const char* ntsFmt, ...)
{
    if (eLevel > s->eLogLevel) return;

    va_list args;
    va_start(args, ntsFmt);
    k_LoggerPostVaList(s, pArena, eLevel, ntsFile, ntsFunc, line, K_NTS(ntsFmt), &args);
    va_end(args);
}

ssize_t
k_LoggerDefaultFormatter(k_Logger* s, void* pArg, K_LOGGER_LEVEL eLevel, const char* ntsFile, const char* ntsFunc, ssize_t line, k_Span spSink)
{
    (void)pArg;

    static const char* mapColored[] = {
        "",
        K_LOGGER_ANSI_COLOR_YELLOW K_LOGGER_ANSI_COLOR_BOLD "WARNING" K_LOGGER_ANSI_COLOR_NORM,
        K_LOGGER_ANSI_COLOR_RED K_LOGGER_ANSI_COLOR_BOLD "ERROR" K_LOGGER_ANSI_COLOR_NORM,
        K_LOGGER_ANSI_COLOR_BLUE K_LOGGER_ANSI_COLOR_ITALIC "INFO" K_LOGGER_ANSI_COLOR_NORM,
        K_LOGGER_ANSI_COLOR_CYAN "DEBUG" K_LOGGER_ANSI_COLOR_NORM,
    };
    static const char* map[] = {
        "",
        "WARNING",
        "ERROR",
        "INFO",
        "DEBUG",
    };
    const char* ntsLevel = "";
    if (eLevel <= K_LOGGER_LEVEL_DEBUG)
    {
        if (s->eFlags & K_LOGGER_FLAG_COLORS) ntsLevel = mapColored[eLevel];
        else ntsLevel = map[eLevel];
    }
    const ssize_t len = strlen(ntsLevel);

    char aTimeBuff[64];
    ssize_t timeBuffSize = 0;
    if (s->eFlags & K_LOGGER_FLAG_TIME)
    {
        time_t now = time(NULL);

#ifdef _WIN32
        struct tm* pTm = localtime(&now);
#else
        struct tm timeStruct = {0};
        struct tm* pTm = localtime_r(&now, &timeStruct);
#endif

        timeBuffSize = strftime(aTimeBuff, sizeof(aTimeBuff), "%Y-%m-%d %I:%M:%S%p", pTm);
    }

    if (s->eFlags & K_LOGGER_FLAG_SOURCE)
    {
        const ssize_t fileSize = strlen(ntsFile);

        return k_print_toBuffer(
            spSink.pData, spSink.size,
            "[{s}{s}" "{PSv}{s}" "{s}{s}" "{s}{s}" "{sz}]: ",
            ntsLevel, len > 0 ? ", " : "",
            &(k_StringView){aTimeBuff, timeBuffSize}, timeBuffSize > 0 ? ", " : "",
            ntsFile, fileSize > 0 ? ", " : "",
            s->eFlags & K_LOGGER_FLAG_FUNC ? ntsFunc : "", s->eFlags & K_LOGGER_FLAG_FUNC ? ", " : "",
            line
        );
    }
    else
    {
        return k_print_toBuffer(
            spSink.pData, spSink.size,
            "[{s}{s}" "{PSv}]: ",
            ntsLevel, len > 0 && timeBuffSize > 0 ? ", " : "",
            &(k_StringView){aTimeBuff, timeBuffSize}
        );
    }
}

ssize_t
k_LoggerDefaultSink(k_Logger* s, void* pArg, k_Span sp)
{
    (void)pArg;
    return k_file_write(s->fd, sp.pData, sp.size);
}

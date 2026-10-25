#pragma once

#include "ThreadPool.h"
#include "Logger.h"
#include "print.h"

/* Usefull runtime globals. */
typedef struct k_Ctx
{
    k_ThreadPool threadPool;
    k_Logger logger;
    k_print_Map* pPrintMap;
} k_Ctx;

extern k_Ctx* k_g_pContext;

k_Ctx* k_CtxAllocGlobal(k_LoggerInitOpts loggerOpts, k_ThreadPoolInitOpts threadPoolOpts);
static inline k_Ctx* k_CtxSetGlobal(k_Ctx* s);
static inline k_Ctx* k_CtxInst(void);
static inline k_Arena* k_CtxArena(void);
static inline k_ThreadPool* k_CtxThreadPool(void);
static inline k_Logger* k_CtxLogger(void);
static inline k_print_Map* k_CtxPrintMap(void);
k_Arena* k_CtxInitArenaForThisThread(ssize_t reserveSize);
void k_CtxDestroyArenaForThisThread(void);
void k_CtxDestroyGlobal(void);

static inline k_Ctx*
k_CtxSetGlobal(k_Ctx* s)
{
    return k_g_pContext = s;
}

static inline k_Ctx*
k_CtxInst(void)
{
    return k_g_pContext;
}

static inline k_Arena*
k_CtxArena(void)
{
    return k_ThreadPoolArena(&k_g_pContext->threadPool);
}

static inline k_ThreadPool*
k_CtxThreadPool(void)
{
    return &k_g_pContext->threadPool;
}

static inline k_Logger*
k_CtxLogger(void)
{
    return &k_g_pContext->logger;
}

static inline k_print_Map*
k_CtxPrintMap(void)
{
    return k_g_pContext->pPrintMap;
}

#if !defined K_CTX_LOG_LEVEL || (K_CTX_LOG_LEVEL >= 1)
    #define K_CTX_LOG_ERROR(...) k_LoggerPost(k_CtxLogger(), k_CtxArena(), K_LOGGER_LEVEL_ERROR, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
    #define K_CTX_LOG_ERROR(...) (void)0
#endif

#if !defined K_CTX_LOG_LEVEL || (K_CTX_LOG_LEVEL >= 2)
    #define K_CTX_LOG_WARN(...) k_LoggerPost(k_CtxLogger(), k_CtxArena(), K_LOGGER_LEVEL_WARNING, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
    #define K_CTX_LOG_WARN(...) (void)0
#endif

#if !defined K_CTX_LOG_LEVEL || (K_CTX_LOG_LEVEL >= 3)
    #define K_CTX_LOG_INFO(...) k_LoggerPost(k_CtxLogger(), k_CtxArena(), K_LOGGER_LEVEL_INFO, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
    #define K_CTX_LOG_INFO(...) (void)0
#endif

#if !defined K_CTX_LOG_LEVEL || (K_CTX_LOG_LEVEL >= 4)
    #define K_CTX_LOG_DEBUG(...) k_LoggerPost(k_CtxLogger(), k_CtxArena(), K_LOGGER_LEVEL_DEBUG, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
    #define K_CTX_LOG_DEBUG(...) (void)0
#endif

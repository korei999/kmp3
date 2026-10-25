#include "Ctx.h"

#include "Gpa.h"

k_Ctx* k_g_pContext;

k_Ctx*
k_CtxAllocGlobal(k_LoggerInitOpts loggerOpts, k_ThreadPoolInitOpts threadPoolOpts)
{
    k_Ctx* pNew = calloc(1, sizeof(k_Ctx));
    k_Gpa* pGpa = k_GpaInst();
    if (!pNew) return NULL;

    k_print_Map* pPrintMap = k_print_MapAlloc(&pGpa->base);
    if (!pPrintMap)
    {
        free(pNew);
        return NULL;
    }

    k_print_MapSetGlobal(pPrintMap);
    pNew->pPrintMap = pPrintMap;

    if (loggerOpts.ringBufferSize > 0)
    {
        if (!k_LoggerInit(&pNew->logger, &pGpa->base, loggerOpts))
        {
            k_print_MapDealloc(&pNew->pPrintMap);
            free(pNew);
            return NULL;
        }
    }

    if (!k_ThreadPoolInit(&pNew->threadPool, threadPoolOpts))
    {
        k_LoggerDestroy(&pNew->logger);
        k_print_MapDealloc(&pNew->pPrintMap);
        free(pNew);
    }

    return k_g_pContext = pNew;
}

k_Arena*
k_CtxInitArenaForThisThread(ssize_t reserveSize)
{
    k_Arena* pArena = k_ThreadPoolArena(&k_g_pContext->threadPool);
    k_ArenaInit(pArena, reserveSize, 0);
    uint8_t** pp = k_ThreadPoolBuffer();
    *pp = calloc(1, k_QueueMPMCSlotSize(&k_g_pContext->threadPool.qTasks));
    return pArena;
}

void
k_CtxDestroyArenaForThisThread(void)
{
    k_ArenaDestroy(k_ThreadPoolArena(&k_g_pContext->threadPool));
    uint8_t** pp = k_ThreadPoolBuffer();
    free(*pp);
    *pp = NULL;
}

void
k_CtxDestroyGlobal(void)
{
    k_LoggerDestroy(&k_g_pContext->logger);
    k_ThreadPoolDestroy(&k_g_pContext->threadPool);
    k_print_MapDealloc(&k_g_pContext->pPrintMap);
    free(k_g_pContext);
    k_g_pContext = NULL;
}

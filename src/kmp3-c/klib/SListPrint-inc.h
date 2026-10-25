/* An attempt to automate custom formatters for iterable structures. */

#ifndef K_NAME
    #error "K_NAME is not defined"
#endif

#ifndef K_PFORMATTER
    #error "K_PFORMATTER is not defined"
#endif

static ssize_t
K_NAME(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    ssize_t nn = 0;
    nn += k_print_BuilderPushChar(pCtx->pBuilder, '[');

    k_print_FmtArgs fmtArgs = *pFmtArgs;
    k_SList* pList = (void*)arg;

    if (pList && pList->pFirst)
    {
        k_SListNode* pNode = pList->pFirst;
        nn += K_PFORMATTER(pCtx, &fmtArgs, pNode + 1);

        while ((pNode = pNode->pNext))
        {
            nn += k_print_BuilderPushSv(pCtx->pBuilder, K_SV(", "));
            nn += K_PFORMATTER(pCtx, &fmtArgs, pNode + 1);
        }
    }

    nn += k_print_BuilderPushChar(pCtx->pBuilder, ']');
    return nn;
}

#undef K_NAME
#undef K_PFORMATTER

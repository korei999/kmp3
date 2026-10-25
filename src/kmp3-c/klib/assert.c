#include "assert.h"

#include "Ctx.h"

_Noreturn void
k_assert_die(const char* ntsFile, const char* ntsFunc, ssize_t line, const char* ntsFmt, ...)
{
    k_Arena* pArena = k_CtxArena();
    char aBuff[128];
    k_print_Builder pb;
    bool b = k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){
        .pAllocOrNull = &pArena->base,
        .pBufferOrNull = aBuff,
        .preallocOrBufferSize = sizeof(aBuff),
    });
    k_StringView svPrinted = {0};
    if (b)
    {
        va_list args;
        va_start(args, ntsFmt);
        k_print_FmtArgs fmtArgs = k_print_FmtArgsCreate();
        k_print_BuilderPrintVaList(&pb, &fmtArgs, K_NTS(ntsFmt), &args);
        va_end(args);
        svPrinted = k_print_BuilderToSv(&pb);

        k_LoggerPostSv(k_CtxLogger(), pArena, K_LOGGER_LEVEL_ERROR, ntsFile, ntsFunc, line, svPrinted);
    }

    k_LoggerDestroy(k_CtxLogger());

#ifdef _WIN32
    MessageBoxA(
        NULL,
        svPrinted.pData,
        "Unrecoverable Error",
        MB_ICONWARNING | MB_OK | MB_DEFBUTTON2
    );

    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif

    abort();
}

#pragma once

#include "klib/print.h"

typedef enum
{
    input_READ_MODE_NONE,
    input_READ_MODE_SEARCH,
    input_READ_MODE_SEEK
} input_READ_MODE;

ssize_t input_ReadModePFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);

typedef enum
{
    input_READ_STATUS_GOOD,
    input_READ_STATUS_DONE,
    input_READ_STATUS_BACKSPACE,
    input_READ_STATUS_TIMEOUT
} input_READ_STATUS;

typedef struct
{
    wchar_t aBuff[128];
    uint32_t idx;
    input_READ_MODE eCurrMode;
    input_READ_MODE eLastUsedMode;
} input_Buffer;

void input_BufferZeroOut(input_Buffer* s);

void input_BufferSubStringSearch(
    input_Buffer* s,
    int16_t* pFirstIdx,
    input_READ_STATUS (*pfnRead)(void* pArg),
    void* pReadArg,
    void (*pfnDraw)(void* pArg),
    void* pDrawArg
);
void input_BufferSeekFromInput(
    input_Buffer* s,
    input_READ_STATUS (*pfnRead)(void* pArg),
    void* pReadArg,
    void (*pfnDraw)(void* pArg),
    void* pDrawArg
);

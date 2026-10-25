#pragma once

#include "StringView.h"
#include "IAllocator.h"

#include <stdio.h>

typedef enum K_CMD_LINE_RESULT
{
    K_CMD_LINE_RESULT_SUCCESS, /* Immediately break and return SUCCESS. */
    K_CMD_LINE_RESULT_FAIL, /* Immediately break and return FAIL. */
    K_CMD_LINE_RESULT_NEXT, /* Proceed to next arg. */
    K_CMD_LINE_RESULT_BREAK, /* Immediately break and return BREAK. */
} K_CMD_LINE_RESULT;

typedef struct k_CmdLine k_CmdLine;
typedef struct k_CmdLineArg k_CmdLineArg;

typedef K_CMD_LINE_RESULT (*k_CmdLineArgValueHandlerPfn)(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg, const k_StringView svValue);
typedef K_CMD_LINE_RESULT (*k_CmdLineArgHandlerPfn)(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg);

struct k_CmdLineArg
{
    union
    {
        k_CmdLineArgValueHandlerPfn pfnValueHandler; /* union with pfnHandler. */
        k_CmdLineArgHandlerPfn pfnHandler; /* union with pfnValueHandler. */
    };
    void* pHandlerArg;
    k_StringView svLongName;
    k_StringView svDescription;
    char cShortName;
    bool bNeedsValue; /* NOTE: Must use pfnValueHandler if true. */
};

void k_CmdLineArgPrintDescription(k_CmdLineArg* s, k_IAllocator* pAlloc, FILE* pFile);

K_NO_DISCARD k_CmdLine* k_CmdLineAlloc(
    k_IAllocator* pAlloc,
    FILE* pFile,
    const k_StringView svName,
    const k_StringView svUsageDescription,
    k_CmdLineArg* pArgs,
    ssize_t nArgs
);
k_IAllocator* k_CmdLineGetAllocator(k_CmdLine* s);
K_CMD_LINE_RESULT k_CmdLineParse(k_CmdLine* s, int argc, char** argv);
void k_CmdLinePrintDescriptions(k_CmdLine* s, k_IAllocator* pAlloc, FILE* pFile);
void k_CmdLinePrintUsage(k_CmdLine* s, k_IAllocator* pAlloc, FILE* pFile);
void k_CmdLineDealloc(k_CmdLine** ps);

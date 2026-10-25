#include "CmdLine.h"

#include "print.h"

#define K_NAME MapSvToIdx
#define K_KEY_T k_StringView
#define K_VALUE_T ssize_t
#define K_FN_HASH k_StringViewHash
#define K_FN_KEY_CMP k_StringViewCmp
#include "MapGen-inc.h"

typedef struct CmdCommand
{
    k_CmdLine* pCmdLine;
    k_CmdLineArg* pCmdArg;
    k_StringView svValue;
} CmdCommand;

#include "Vec.h"

struct k_CmdLine
{
    MapSvToIdx mapSvToIdxs;
    k_IAllocator* pAlloc;
    k_CmdLineArg* pArgs;
    ssize_t nArgs;
    FILE* pFile;
    k_StringView svName;
    k_StringView svUsageDescription;
    k_Vec vCmdCommands; /* <CmdCommand> */
};

void
k_CmdLineArgPrintDescription(k_CmdLineArg* s, k_IAllocator* pAlloc, FILE* pFile)
{
    if (s->svDescription.size <= 0 && !s->cShortName && s->svLongName.size <= 0) return;

    char aBuff[128];
    k_print_Builder pb;
    k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){
        .pAllocOrNull = pAlloc,
        .pBufferOrNull = aBuff,
        .preallocOrBufferSize = sizeof(aBuff),
    });
    if (s->cShortName != '\0') k_print_BuilderPrint(&pb, "    -{c}", s->cShortName);
    if (s->cShortName != '\0' && s->svLongName.size > 0) k_print_BuilderPushSv(&pb, K_SV(", "));
    if (s->cShortName == '\0' && s->svLongName.size > 0) k_print_BuilderPushSv(&pb, K_SV("    "));
    if (s->svLongName.size > 0) k_print_BuilderPrint(&pb, "--{PSv}", &s->svLongName);
    if (s->bNeedsValue) k_print_BuilderPushSv(&pb, K_SV("=[value]"));
    k_print_BuilderPrint(&pb, "\n        {PSv}\n", &s->svDescription);
    k_print_BuilderFlush(&pb, pFile);
}

k_CmdLine*
k_CmdLineAlloc(k_IAllocator* pAlloc, FILE* pFile, const k_StringView svName, const k_StringView svUsageDescription, k_CmdLineArg* pArgs, ssize_t nArgs)
{
    if (nArgs <= 0) return NULL;

    k_CmdLine* s = k_IAllocatorMalloc(pAlloc, sizeof(k_CmdLine));
    if (!s) return NULL;

    if (!MapSvToIdxInit(&s->mapSvToIdxs, pAlloc, nArgs))
    {
        k_IAllocatorFree(pAlloc, s);
        return NULL;
    }

    s->pAlloc = pAlloc;
    s->pArgs = pArgs;
    s->nArgs = nArgs;
    s->pFile = pFile;
    s->svName = svName;
    s->svUsageDescription = svUsageDescription;

    for (ssize_t i = 0; i < nArgs; ++i)
    {
        k_CmdLineArg* pArg = &pArgs[i];
        MapSvToIdxInsert(&s->mapSvToIdxs, pAlloc, &(k_StringView){&pArg->cShortName, 1}, &(ssize_t){i});
        MapSvToIdxInsert(&s->mapSvToIdxs, pAlloc, &pArg->svLongName, &(ssize_t){i});
    }

    if (!k_VecInit(&s->vCmdCommands, pAlloc, sizeof(CmdCommand), nArgs))
    {
        MapSvToIdxDestroy(&s->mapSvToIdxs, pAlloc);
        k_IAllocatorFree(pAlloc, s);
        return NULL;
    }

    return s;
}

k_IAllocator*
k_CmdLineGetAllocator(k_CmdLine* s)
{
    return s->pAlloc;
}

static K_CMD_LINE_RESULT
parseShort(k_CmdLine* s, int* pI, int argc, char** argv)
{
    const k_StringView svArg = k_StringViewSubString1(K_NTS(argv[*pI]), 1);

    for (ssize_t charI = 0; charI < svArg.size; ++charI)
    {
        MapSvToIdxResult mapRes = MapSvToIdxSearch(&s->mapSvToIdxs, &(k_StringView){svArg.pData + charI, 1});
        if (mapRes.eStatus == K_MAP_RESULT_STATUS_FOUND)
        {
            ssize_t argI = mapRes.pBucket->value;
            k_CmdLineArg* pCmdArg = &s->pArgs[argI];
            if (pCmdArg->bNeedsValue)
            {
                if (charI + 1 < svArg.size && svArg.pData[charI + 1] == '=')
                {
                    k_StringView svVal = k_StringViewSubString1(svArg, charI + 2);
                    k_VecPush(&s->vCmdCommands, s->pAlloc, sizeof(CmdCommand), &(CmdCommand){s, pCmdArg, svVal});
                    break;
                }
                else if (svArg.size > 1)
                {
                    k_print(s->pAlloc, s->pFile, "Arg '{c}' needs value but in was used inside the pack '{PSv}'.\n", svArg.pData[charI], &svArg);
                    k_CmdLineArgPrintDescription(pCmdArg, s->pAlloc, s->pFile);
                    return K_CMD_LINE_RESULT_FAIL;
                }
                else if (*pI + 1 >= argc)
                {
                    k_print(s->pAlloc, s->pFile, "No value for arg '{c}'.\n", svArg.pData[charI]);
                    k_CmdLineArgPrintDescription(pCmdArg, s->pAlloc, s->pFile);
                    return K_CMD_LINE_RESULT_FAIL;
                }
                else
                {
                    k_VecPush(&s->vCmdCommands,  s->pAlloc, sizeof(CmdCommand), &(CmdCommand){s, pCmdArg, K_NTS(argv[*pI + 1])});
                    ++(*pI);
                }
            }
            else
            {
                k_VecPush(&s->vCmdCommands, s->pAlloc, sizeof(CmdCommand), &(CmdCommand){.pCmdLine = s, .pCmdArg = pCmdArg});
            }
        }
        else
        {
            k_print(s->pAlloc, s->pFile, "Unknown arg '{c}'.\n", svArg.pData[charI]);
            return K_CMD_LINE_RESULT_FAIL;
        }
    }

    return K_CMD_LINE_RESULT_NEXT;
}

static K_CMD_LINE_RESULT
parseLong(k_CmdLine* s, int i, char** argv)
{
    const k_StringView svArg = k_StringViewSubString1(K_NTS(argv[i]), 2);
    const ssize_t equalsI = k_StringViewCharAt(svArg, '=');

    MapSvToIdxResult mapRes;
    if (equalsI <= -1)
    {
        mapRes = MapSvToIdxSearch(&s->mapSvToIdxs, &svArg);
    }
    else
    {
        const k_StringView svNoEq = k_StringViewSubString(svArg, 0, equalsI);
        mapRes = MapSvToIdxSearch(&s->mapSvToIdxs, &svNoEq);
    }

    if (mapRes.eStatus == K_MAP_RESULT_STATUS_FOUND)
    {
        k_CmdLineArg* pCmdArg = &s->pArgs[mapRes.pBucket->value];
        if (pCmdArg->bNeedsValue)
        {
            if (equalsI <= -1)
            {
                k_print(s->pAlloc, s->pFile, "No '=' sign after '{PSv}'.\n", &svArg);
                k_CmdLineArgPrintDescription(pCmdArg, s->pAlloc, s->pFile);
                return K_CMD_LINE_RESULT_FAIL;
            }
            else
            {
                const k_StringView svVal = k_StringViewSubString1(svArg, equalsI + 1);
                k_VecPush(&s->vCmdCommands, s->pAlloc, sizeof(CmdCommand), &(CmdCommand){s, pCmdArg, svVal});
                return K_CMD_LINE_RESULT_NEXT;
            }
        }
        else
        {
            k_VecPush(&s->vCmdCommands, s->pAlloc, sizeof(CmdCommand), &(CmdCommand){.pCmdLine = s, .pCmdArg = pCmdArg});
            return K_CMD_LINE_RESULT_NEXT;
        }
    }
    else
    {
        k_print(s->pAlloc, s->pFile, "Unknown arg '{PSv}'.\n", &svArg);
        return K_CMD_LINE_RESULT_FAIL;
    }

    return K_CMD_LINE_RESULT_NEXT;
}

K_CMD_LINE_RESULT
k_CmdLineParse(k_CmdLine* s, int argc, char** argv)
{
    K_CMD_LINE_RESULT eRes = K_CMD_LINE_RESULT_NEXT;
    for (int i = 0; i < argc;)
    {
        const k_StringView svArg = K_NTS(argv[i]);
        if (k_StringViewStartsWith(svArg, K_SV("--")))
            eRes = parseLong(s, i, argv);
        else if (k_StringViewStartsWith(svArg, K_SV("-")))
            eRes = parseShort(s, &i, argc, argv);
        else if (i > 0) return K_CMD_LINE_RESULT_SUCCESS;

        if (eRes != K_CMD_LINE_RESULT_NEXT) goto done;
        ++i;
    }

    eRes = K_CMD_LINE_RESULT_SUCCESS;
    for (ssize_t i = 0; i < s->vCmdCommands.size; ++i)
    {
        CmdCommand* pCmd = k_VecGetP(&s->vCmdCommands, sizeof(CmdCommand), i);
        if (pCmd->pCmdArg->bNeedsValue)
            eRes = pCmd->pCmdArg->pfnValueHandler(pCmd->pCmdLine, pCmd->pCmdArg, pCmd->svValue);
        else eRes = pCmd->pCmdArg->pfnHandler(pCmd->pCmdLine, pCmd->pCmdArg);

        if (eRes != K_CMD_LINE_RESULT_NEXT) break;
    }

done:
    k_VecDestroy(&s->vCmdCommands, s->pAlloc);
    if (eRes == K_CMD_LINE_RESULT_NEXT || eRes == K_CMD_LINE_RESULT_SUCCESS)
        return eRes = K_CMD_LINE_RESULT_SUCCESS;
    else return eRes;
}

void
k_CmdLinePrintDescriptions(k_CmdLine* s, k_IAllocator* pAlloc, FILE* pFile)
{
    if (s->nArgs <= 0) return;

    k_CmdLinePrintUsage(s, pAlloc, pFile);
    for (ssize_t i = 0; i < s->nArgs; ++i)
    {
        k_CmdLineArgPrintDescription(s->pArgs + i, pAlloc, pFile);
        if (i < s->nArgs - 1) k_print(pAlloc, pFile, "\n");
    }
}

void
k_CmdLinePrintUsage(k_CmdLine* s, k_IAllocator* pAlloc, FILE* pFile)
{
    k_print(pAlloc, pFile, "Usage: {PSv}{s}{PSv}\n\n", &s->svName, s->svName.size > 0 ? " " : "", &s->svUsageDescription);
}

void
k_CmdLineDealloc(k_CmdLine** ps)
{
    if (!ps || !*ps) return;

    MapSvToIdxDestroy(&(*ps)->mapSvToIdxs, (*ps)->pAlloc);
    k_IAllocatorFree((*ps)->pAlloc, *ps);
    *ps = NULL;
}

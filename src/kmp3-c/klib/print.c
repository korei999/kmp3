#include "print.h"
#include "String.h"

#include "ThirdParty/ryu/ryu.h"

#include <ctype.h>
#include <math.h>

#define K_NAME MapSigToPfn
#define K_KEY_T k_StringView
#define K_VALUE_T k_print_PfnFormat
#define K_FN_HASH k_StringViewHash
#define K_FN_KEY_CMP k_StringViewCmp
#define K_DECL_MOD K_UNUSED static
#define K_GEN_DECLS
#define K_GEN_CODE
#include "MapGen-inc.h"

struct k_print_Map
{
    k_IAllocator* pAlloc;
    MapSigToPfn mSigToPfn;
};

static k_print_Map* s_pGlobalPrinter = NULL;

static ssize_t parseVaList(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs);
static ssize_t parseArg(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs);
static void parseFmtArg(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs);
static void parseRangeArgs(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs);
static void parseMemberSizeArg(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs);
static ssize_t execFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const k_StringView* pSvKey, va_list* pArgs);
static ssize_t execArrayFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, k_print_PfnFormat pfn, va_list* pArgs);
ssize_t k_print_eatFmtArg(k_print_FmtArgs* pFmtArgs, int64_t num);

static bool
k_print_BuilderGrow(k_print_Builder* s, ssize_t newCap)
{
    char* pNewData = NULL;

    if (!s->bAllocated)
    {
        pNewData = K_IMALLOC_T(s->pAlloc, char, newCap);
        if (!pNewData) return false;

        s->bAllocated = true;
        /* Ignore asan. */
        memcpy(pNewData, s->pData, s->size);
    }
    else
    {
        pNewData = K_IREALLOC_T(s->pAlloc, char, s->pData, s->cap, newCap);
        if (!pNewData) return false;
    }

    s->cap = newCap;
    s->pData = pNewData;
    return true;
}

k_print_FmtArgs
k_print_FmtArgsCreate(void)
{
    return (k_print_FmtArgs){
        .maxLen = K_NPOS,
        .padSize = 0,
        .maxFloatLen = K_NPOS,
        .memberSize = K_NPOS64,
        .arraySize = K_NPOS64,

        .eBase = K_PRINT_BASE_TEN,
        .filler = ' ',
    };
}

k_print_Map*
k_print_MapAlloc(k_IAllocator* pAlloc)
{
    k_print_Map* pMap = K_IALLOC(pAlloc, k_print_Map, .pAlloc = pAlloc);
    if (!pMap) return NULL;

    if (!MapSigToPfnInit(&pMap->mSigToPfn, pAlloc, 128))
    {
        k_IAllocatorFree(pAlloc, pMap);
        return NULL;
    }

    k_print_MapAddDefaultFormatters(pMap);

    return pMap;
}

void
k_print_MapSetGlobal(k_print_Map* pPrinter)
{
    s_pGlobalPrinter = pPrinter;
}

k_print_Map*
k_print_MapInst(void)
{
    return s_pGlobalPrinter;
}

bool
k_print_MapAddFormatter(k_print_Map* s, const char* ntsSignature, k_print_PfnFormat pfnFormat)
{
    MapSigToPfnResult r = MapSigToPfnInsert(&s->mSigToPfn, s->pAlloc, &K_NTS(ntsSignature), &pfnFormat);
    if (r.eStatus != K_MAP_RESULT_STATUS_FAILED) return true;
    else return false;
}

bool
k_print_MapAddFormatterSv(k_print_Map* s, const k_StringView svSignature, k_print_PfnFormat pfnFormat)
{
    MapSigToPfnResult r = MapSigToPfnInsert(&s->mSigToPfn, s->pAlloc, &svSignature, &pfnFormat);
    if (r.eStatus != K_MAP_RESULT_STATUS_FAILED) return true;
    else return false;
}

bool
k_print_MapRemoveFormatter(k_print_Map* s, const char* ntsSignature)
{
    MapSigToPfnRemove(&s->mSigToPfn, &K_NTS(ntsSignature));
    return true;
}

bool
k_print_MapRemoveFormatterSv(k_print_Map* s, const k_StringView svSignature)
{
    MapSigToPfnRemove(&s->mSigToPfn, &svSignature);
    return true;
}

void
k_print_MapDestroy(k_print_Map* s)
{
    MapSigToPfnDestroy(&s->mSigToPfn, s->pAlloc);
}

void
k_print_MapDealloc(k_print_Map** ps)
{
    k_print_MapDestroy(*ps);
    k_IAllocatorFree((*ps)->pAlloc, *ps);
    *ps = NULL;
}

bool
k_print_BuilderInit(k_print_Builder* s, k_print_BuilderInitOpts opts)
{
    s->pAlloc = opts.pAllocOrNull;
    s->pData = opts.pBufferOrNull;
    s->size = 0;
    s->cap = 0;
    s->bAllocated = false;

    if (opts.pAllocOrNull && !opts.pBufferOrNull && opts.preallocOrBufferSize > 0)
    {
        s->pData = K_IMALLOC_T(s->pAlloc, char, opts.preallocOrBufferSize);
        if (!s->pData) return false;
        s->bAllocated = true;
        s->cap = opts.preallocOrBufferSize;
    }
    else if (opts.pBufferOrNull)
    {
        s->cap = opts.preallocOrBufferSize;
    }

    return true;
}

void
k_print_BuilderDestroy(k_print_Builder* s)
{
    if (s->bAllocated) k_IAllocatorFree(s->pAlloc, s->pData);
}

ssize_t
k_print_BuilderPushSvPadded(k_print_Builder* s, const k_StringView sv, k_print_paddingOpts opts)
{
    if (sv.size >= opts.size) return k_print_BuilderPushSv(s, sv);

    const ssize_t maxSize = K_MAX(sv.size, opts.size);
    const ssize_t padSize = maxSize - sv.size;

    if (s->size + maxSize >= s->cap)
    {
        ssize_t newCap = s->cap * 2;
        if (s->size + maxSize >= newCap) newCap = s->size + maxSize + 1;
        if (s->pAlloc && k_print_BuilderGrow(s, newCap))
        {
            /* Noop. */
        }
        else
        {
            /* TODO: fit as much as possible (Who cares?). */
            return 0;
        }
    }

    if (opts.bJustifyRight)
    {
        memset(s->pData + s->size, opts.filler, padSize);
        s->size += padSize;
        memcpy(s->pData + s->size, sv.pData, sv.size);
        s->size += sv.size;
    }
    else
    {
        memcpy(s->pData + s->size, sv.pData, sv.size);
        s->size += sv.size;
        memset(s->pData + s->size, opts.filler, padSize);
        s->size += padSize;
    }

    s->pData[s->size] = '\0';
    return maxSize;
}

ssize_t
k_print_BuilderPushSvPaddedFmtArgs(k_print_Builder* s, k_print_FmtArgs* pFmtArgs, const k_StringView sv)
{
    ssize_t maxLen = pFmtArgs->maxLen >= 0 ? pFmtArgs->maxLen : sv.size;
    ssize_t maxSvLen = K_MIN(maxLen, sv.size);

    ssize_t n = k_print_BuilderPushSvPadded(s, (k_StringView){sv.pData, maxSvLen}, (k_print_paddingOpts){
        .size = pFmtArgs->padSize == 0 ? (maxLen) : pFmtArgs->padSize,
        .bJustifyRight = pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_JUSTIFY_RIGHT,
        .filler = pFmtArgs->filler,
    });

    pFmtArgs->eFmtFlags &= ~K_PRINT_FMT_FLAGS_JUSTIFY_RIGHT;
    pFmtArgs->eFmtFlags &= ~K_PRINT_FMT_FLAGS_JUSTIFY_LEFT;

    return n;
}

ssize_t
k_print_BuilderPush(k_print_Builder* s, const char* pStr, ssize_t size)
{
    if (size <= 0 || !pStr) return 0;

    if (s->size + size >= s->cap)
    {
        ssize_t newCap = s->cap * 2;
        if (s->size + size >= newCap) newCap = s->size + size + 1;
        if (s->pAlloc && k_print_BuilderGrow(s, newCap))
        {
            //
        }
        else
        {
            const ssize_t maxFit = s->cap - 1 - s->size;
            if (maxFit > 0)
            {
                memcpy(s->pData + s->size, pStr, maxFit);
                s->size += maxFit;
                s->pData[s->size] = '\0';
                return maxFit;
            }
            else
            {
                return 0;
            }
        }
    }

    memcpy(s->pData + s->size, pStr, size);
    s->size += size;
    s->pData[s->size] = '\0';
    return size;
}

ssize_t
k_print_BuilderPushSv(k_print_Builder* s, const k_StringView sv)
{
    return k_print_BuilderPush(s, sv.pData, sv.size);
}

ssize_t
k_print_BuilderPushChar(k_print_Builder* s, const char c)
{
    if (s->size + 1 >= s->cap)
    {
        if (!s->pAlloc) return 0;
        if (!k_print_BuilderGrow(s, K_MAX(8, s->cap * 2)))
            return 0;
    }

    s->pData[s->size++] = c;
    s->pData[s->size] = '\0';
    return 1;
}

void
k_print_BuilderFlush(k_print_Builder* s, FILE* pFile)
{
    const k_StringView svPrinted = k_print_BuilderToSv(s);
    fwrite(svPrinted.pData, svPrinted.size, 1, pFile);
}

k_print_BuilderSV
k_print_BuilderPrintVaList(k_print_Builder* s, k_print_FmtArgs* pFmtArgs, const k_StringView svFmt, va_list* pArgs)
{
    k_print_BuilderSV sv = {.off = s->size, .size = 0};
    k_print_Map* pPrinter = k_print_MapInst();
    if (!pPrinter || svFmt.size <= 0 || !svFmt.pData) return sv;

    k_print_Context ctx = {.pPrinter = pPrinter, .pBuilder = s, .svFmt = svFmt};
    sv.size = parseVaList(&ctx, pFmtArgs, pArgs);
    return sv;
}

k_print_BuilderSV
k_print_BuilderPrintSv(k_print_Builder* s, const k_StringView svFmt, ...)
{
    va_list args;
    va_start(args, svFmt);
    k_print_FmtArgs fmtArgs = k_print_FmtArgsCreate();
    k_print_BuilderSV sv = k_print_BuilderPrintVaList(s, &fmtArgs, svFmt, &args);
    va_end(args);
    return sv;
}

k_print_BuilderSV
k_print_BuilderPrint(k_print_Builder* s, const char* ntsFmt, ...)
{
    va_list args;
    va_start(args, ntsFmt);
    k_print_FmtArgs fmtArgs = k_print_FmtArgsCreate();
    k_print_BuilderSV sv = k_print_BuilderPrintVaList(s, &fmtArgs, K_NTS(ntsFmt), &args);
    va_end(args);
    return sv;
}

k_print_BuilderSV
k_print_BuilderPrintFmtArgs(k_print_Builder* s, k_print_FmtArgs* pFmtArgs, const char* ntsFmt, ...)
{
    va_list args;
    va_start(args, ntsFmt);
    k_print_BuilderSV sv = k_print_BuilderPrintVaList(s, pFmtArgs, K_NTS(ntsFmt), &args);
    va_end(args);
    return sv;
}

k_print_BuilderSV
k_print_BuilderPrintSvFmtArgs(k_print_Builder* s, k_print_FmtArgs* pFmtArgs, k_StringView svFmt, ...)
{
    va_list args;
    va_start(args, svFmt);
    k_print_BuilderSV sv = k_print_BuilderPrintVaList(s, pFmtArgs, svFmt, &args);
    va_end(args);
    return sv;
}

static ssize_t
sayNoFormatter(k_print_Context* pCtx, const k_StringView* pSvKey)
{
    return k_print_BuilderPrint(pCtx->pBuilder,
        "(no formatter for '{PSv}')",
        pSvKey
    ).size;
}

static ssize_t
execFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const k_StringView* pSvKey, va_list* pArgs)
{
    ssize_t nWritten = 0;

    MapSigToPfnResult mapRes = MapSigToPfnSearch(&pCtx->pPrinter->mSigToPfn, pSvKey);
    if (mapRes.eStatus == K_MAP_RESULT_STATUS_FOUND && mapRes.pBucket->value)
    {
        /* NOTE: Doubles/floats are usually passed in XMM registers, using void* directly will not work. */
        if (mapRes.pBucket->value == k_print_formatDouble)
        {
            double d = va_arg(*pArgs, double);
            nWritten = mapRes.pBucket->value(pCtx, pFmtArgs, &d);
        }
        else
        {
            if (pFmtArgs->arraySize > 0 && pFmtArgs->memberSize > 0)
                nWritten = execArrayFormatter(pCtx, pFmtArgs, mapRes.pBucket->value, pArgs);
            else nWritten = mapRes.pBucket->value(pCtx, pFmtArgs, va_arg(*pArgs, void*));
        }
    }
    else
    {
        nWritten = sayNoFormatter(pCtx, pSvKey);
    }

    return nWritten;
}

static ssize_t
execArrayFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, k_print_PfnFormat pfn, va_list* pArgs)
{
    ssize_t nWritten = 0;

    nWritten += k_print_BuilderPushChar(pCtx->pBuilder, '[');

    uint8_t* pRange = va_arg(*pArgs, void*);

    k_print_FmtArgs fmtArgs2 = *pFmtArgs;

    if (pFmtArgs->arraySize > 0)
        nWritten += pfn(pCtx, &fmtArgs2, &pRange[0]);

    for (ssize_t i = 1; i < pFmtArgs->arraySize; ++i)
    {
        fmtArgs2 = *pFmtArgs;
        k_print_BuilderPushSv(pCtx->pBuilder, K_SV(", "));
        nWritten += pfn(pCtx, &fmtArgs2, pRange + i*pFmtArgs->memberSize);
    }

    nWritten += k_print_BuilderPushChar(pCtx->pBuilder, ']');

    return nWritten;
}

ssize_t
k_print_eatFmtArg(k_print_FmtArgs* pFmtArgs, int64_t num)
{
    if (pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_ARG_IS_FLOAT_PRECISION)
    {
        pFmtArgs->eFmtFlags &= ~K_PRINT_FMT_FLAGS_ARG_IS_FLOAT_PRECISION;
        pFmtArgs->maxFloatLen = num;
    }
    else if (pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_ARG_IS_FILLER)
    {
        pFmtArgs->eFmtFlags &= ~K_PRINT_FMT_FLAGS_ARG_IS_FILLER;
        pFmtArgs->filler = num;
    }
    else if (pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_ARG_IS_MEMBER_SIZE)
    {
        pFmtArgs->eFmtFlags &= ~K_PRINT_FMT_FLAGS_ARG_IS_MEMBER_SIZE;
        pFmtArgs->memberSize = num;
    }
    else if (pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_ARG_IS_RANGE)
    {
        pFmtArgs->arraySize = num;
    }
    else if (pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_JUSTIFY_LEFT)
    {
        pFmtArgs->padSize = num;
    }
    else if (pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_JUSTIFY_RIGHT)
    {
        pFmtArgs->padSize = num;
    }
    else
    {
        pFmtArgs->maxLen = num;
    }

    return K_PRINT_FMT_ARG_EATEN;
}

static void
parseNumber(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs)
{
    ssize_t startI = pCtx->fmtI;

    ++pCtx->fmtI;
    while (pCtx->fmtI < pCtx->svFmt.size && isdigit(pCtx->svFmt.pData[pCtx->fmtI]))
        ++pCtx->fmtI;

    const k_StringView svNum = {pCtx->svFmt.pData + startI, pCtx->fmtI - startI};
    const int64_t num = k_StringViewToI64(svNum, 10);
    k_print_eatFmtArg(pFmtArgs, num);
}

static void
parseCharOrArg(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs)
{
    if (pCtx->fmtI < pCtx->svFmt.size)
    {
        if (pCtx->svFmt.pData[pCtx->fmtI] == '{')
        {
            ++pCtx->fmtI;
            parseFmtArg(pCtx, pFmtArgs, pArgs);
        }
        else
        {
            pFmtArgs->filler = pCtx->svFmt.pData[pCtx->fmtI];
            pFmtArgs->eFmtFlags &= ~K_PRINT_FMT_FLAGS_ARG_IS_FILLER;
        }
    }
}

static void
parseFmtArg(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs)
{
    ssize_t startI = pCtx->fmtI;

    while (pCtx->fmtI < pCtx->svFmt.size && pCtx->svFmt.pData[pCtx->fmtI] != '}')
        ++pCtx->fmtI;

    k_StringView svKey = {pCtx->svFmt.pData + startI, pCtx->fmtI - startI};
    assert(pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_ARG_IS_FMT && "must be a colon arg");
    execFormatter(pCtx, pFmtArgs, &svKey, pArgs);
}

static void
parseRangeArgs(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs)
{
    while (pCtx->fmtI < pCtx->svFmt.size && pCtx->svFmt.pData[pCtx->fmtI] != ']')
    {
        if (pCtx->svFmt.pData[pCtx->fmtI] == '(')
        {
            ++pCtx->fmtI;
            pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_ARG_IS_MEMBER_SIZE;
            parseMemberSizeArg(pCtx, pFmtArgs, pArgs);
        }
        else if (isdigit(pCtx->svFmt.pData[pCtx->fmtI]))
        {
            pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_ARG_IS_RANGE;
            parseNumber(pCtx, pFmtArgs);
            continue;
        }
        else if (pCtx->svFmt.pData[pCtx->fmtI] == '{')
        {
            ++pCtx->fmtI;
            pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_ARG_IS_RANGE;
            parseFmtArg(pCtx, pFmtArgs, pArgs);
            continue;
        }

        ++pCtx->fmtI;
    }

    if (pCtx->fmtI < pCtx->svFmt.size && pCtx->svFmt.pData[pCtx->fmtI] == ']')
        ++pCtx->fmtI;

    pFmtArgs->eFmtFlags &= ~K_PRINT_FMT_FLAGS_ARG_IS_FMT;
    pFmtArgs->eFmtFlags &= ~K_PRINT_FMT_FLAGS_ARG_IS_RANGE;
}

static void
parseMemberSizeArg(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs)
{
    while (pCtx->fmtI < pCtx->svFmt.size && pCtx->svFmt.pData[pCtx->fmtI] != ')')
    {
        if (isdigit(pCtx->svFmt.pData[pCtx->fmtI]))
        {
            parseNumber(pCtx, pFmtArgs);
            continue;
        }
        else if (pCtx->svFmt.pData[pCtx->fmtI] == '{')
        {
            ++pCtx->fmtI;
            parseFmtArg(pCtx, pFmtArgs, pArgs);
        }

        ++pCtx->fmtI;
    }

    if (pCtx->fmtI < pCtx->svFmt.size && pCtx->svFmt.pData[pCtx->fmtI] == ')')
        ++pCtx->fmtI;
}

static void
parseFmtArgs(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs)
{
    while (pCtx->fmtI < pCtx->svFmt.size && pCtx->svFmt.pData[pCtx->fmtI] != ':')
    {
        if (isdigit(pCtx->svFmt.pData[pCtx->fmtI]))
        {
            parseNumber(pCtx, pFmtArgs);
            continue; /* Don't increment here to allow dot right after number with no need to separate with space. */
        }

        switch (pCtx->svFmt.pData[pCtx->fmtI])
        {
            case ' ':
            break;

            case '{':
            ++pCtx->fmtI;
            parseFmtArg(pCtx, pFmtArgs, pArgs);
            break;

            case '<':
            pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_JUSTIFY_LEFT;
            break;

            case '>':
            pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_JUSTIFY_RIGHT;
            break;

            case 'f':
            {
                ++pCtx->fmtI;
                pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_ARG_IS_FILLER;
                parseCharOrArg(pCtx, pFmtArgs, pArgs);
            }
            break;

            case '+':
            pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_SHOW_SIGN;
            break;

            case '.':
            pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_ARG_IS_FLOAT_PRECISION;
            break;

            case '#':
            pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_HASH;
            break;

            case 'b':
            pFmtArgs->eBase = K_PRINT_BASE_TWO;
            break;

            case 'o':
            pFmtArgs->eBase = K_PRINT_BASE_EIGHT;
            break;

            case 'x':
            pFmtArgs->eBase = K_PRINT_BASE_SIXTEEN;
            break;
        }

        ++pCtx->fmtI;
    }

    /* Skip colon to get correct svKey offsets. */
    if (pCtx->fmtI < pCtx->svFmt.size && pCtx->svFmt.pData[pCtx->fmtI] == ':')
        ++pCtx->fmtI;

    pFmtArgs->eFmtFlags &= ~K_PRINT_FMT_FLAGS_ARG_IS_FMT;
}

static ssize_t
parseArg(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs)
{
    ssize_t nWritten = 0;
    ssize_t keyI = pCtx->fmtI;

    k_StringView svKey = {0};
    while (pCtx->fmtI < pCtx->svFmt.size && pCtx->svFmt.pData[pCtx->fmtI] != '}')
    {
        if (pCtx->svFmt.pData[pCtx->fmtI] == ':')
        {
            ++pCtx->fmtI;
            pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_ARG_IS_FMT;
            parseFmtArgs(pCtx, pFmtArgs, pArgs);
            keyI = pCtx->fmtI;
        }

        if (pCtx->svFmt.pData[pCtx->fmtI] == '[')
        {
            ++pCtx->fmtI;
            pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_ARG_IS_FMT;
            parseRangeArgs(pCtx, pFmtArgs, pArgs);
            keyI = pCtx->fmtI;
        }

        ++pCtx->fmtI;
    }

    svKey.pData = pCtx->svFmt.pData + keyI;
    svKey.size = pCtx->fmtI - keyI;

    {
        ssize_t nn = execFormatter(pCtx, pFmtArgs, &svKey, pArgs);
        if (nn > 0) nWritten += nn;
    }

    if (pCtx->fmtI < pCtx->svFmt.size) ++pCtx->fmtI;

    return nWritten;
}

static ssize_t
parseVaList(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, va_list* pArgs)
{
    ssize_t nWritten = 0;

    while (pCtx->fmtI < pCtx->svFmt.size)
    {
        char* pPercent = (char*)memchr(pCtx->svFmt.pData + pCtx->fmtI, '{', pCtx->svFmt.size - pCtx->fmtI);
        if (!pPercent)
        {
            nWritten += k_print_BuilderPush(pCtx->pBuilder, pCtx->svFmt.pData + pCtx->fmtI, pCtx->svFmt.size - pCtx->fmtI);
            goto done;
        }

        ssize_t nextDiff = pPercent - (pCtx->svFmt.pData + pCtx->fmtI);

        if (nextDiff > 0)
        {
            ssize_t nn = k_print_BuilderPush(pCtx->pBuilder, pCtx->svFmt.pData + pCtx->fmtI, nextDiff);
            if (nn <= 0) goto done;
            pCtx->fmtI += nextDiff;
            nWritten += nn;
        }

        ++pCtx->fmtI;

        if (pCtx->fmtI < pCtx->svFmt.size)
        {
            if (pCtx->svFmt.pData[pCtx->fmtI] != '{') /* Skip formatter if double {{. */
            {
                if (pCtx->fmtI < pCtx->svFmt.size)
                {
                    k_print_FmtArgs fmtArgs2 = *pFmtArgs;
                    nWritten += parseArg(pCtx, &fmtArgs2, pArgs);
                }
                else
                {
                    goto done;
                }
            }
            else
            {
                if (k_print_BuilderPushChar(pCtx->pBuilder, pCtx->svFmt.pData[pCtx->fmtI]) <= 0)
                    goto done;

                ++pCtx->fmtI;
                ++nWritten;
            }
        }
    }

done:
    if (pCtx->pBuilder->size < pCtx->pBuilder->cap)
        pCtx->pBuilder->pData[pCtx->pBuilder->size] = '\0';
    return nWritten;
}

ssize_t
k_print_toBufferVaList(char* pBuff, ssize_t bufferSize, const k_StringView svFmt, va_list* pArgs)
{
    k_print_Map* pPrinter = k_print_MapInst();
    if (!pPrinter || svFmt.size <= 0 || !svFmt.pData) return 0;

    k_print_Builder builder;
    k_print_BuilderInitOpts opts = {.pBufferOrNull = pBuff, .preallocOrBufferSize = bufferSize};
    if (!k_print_BuilderInit(&builder, opts)) return 0;

    k_print_Context ctx = {.pPrinter = pPrinter, .pBuilder = &builder, .svFmt = svFmt, .fmtI = 0};

    ssize_t nWritten = 0;
    k_print_FmtArgs fmtArgs = k_print_FmtArgsCreate();
    nWritten = parseVaList(&ctx, &fmtArgs, pArgs);

    return nWritten;
}

ssize_t
k_print_toBufferSv(char* pBuff, ssize_t bufferSize, const k_StringView svFmt, ...)
{
    va_list args;
    va_start(args, svFmt);
    ssize_t n = k_print_toBufferVaList(pBuff, bufferSize, svFmt, &args);
    va_end(args);
    return n;
}

ssize_t
k_print_toBuffer(char* pBuff, ssize_t bufferSize, const char* ntsFmt, ...)
{
    va_list args;
    va_start(args, ntsFmt);
    ssize_t n = k_print_toBufferVaList(pBuff, bufferSize, K_NTS(ntsFmt), &args);
    va_end(args);
    return n;
}


ssize_t
k_print_VaList(k_IAllocator* pAlloc, FILE* pFile, char* pBuff, ssize_t bufferSize, const k_StringView svFmt, va_list* pArgs)
{
    k_print_Map* pPrinter = k_print_MapInst();
    if (!pPrinter || svFmt.size <= 0 || !svFmt.pData) return 0;

    k_print_Builder builder;
    k_print_BuilderInitOpts opts = {.pBufferOrNull = pBuff, .preallocOrBufferSize = bufferSize, .pAllocOrNull = pAlloc};
    if (!k_print_BuilderInit(&builder, opts)) return 0;

    k_print_Context ctx = {.pPrinter = pPrinter, .pBuilder = &builder, .svFmt = svFmt, .fmtI = 0};

    ssize_t nWritten = 0;
    k_print_FmtArgs fmtArgs = k_print_FmtArgsCreate();
    nWritten = parseVaList(&ctx, &fmtArgs, pArgs);

    fwrite(builder.pData, builder.size, 1, pFile);
    k_print_BuilderDestroy(&builder);

    return nWritten;
}

ssize_t
k_print_Sv(k_IAllocator* pAlloc, FILE* pFile, const k_StringView svFmt, ...)
{
    va_list args;
    va_start(args, svFmt);
    char aBuff[K_PRINT_PREALLOC_SIZE];
    ssize_t n = k_print_VaList(pAlloc, pFile, aBuff, sizeof(aBuff), svFmt, &args);
    va_end(args);
    return n;
}

ssize_t
k_print(k_IAllocator* pAlloc, FILE* pFile, const char* nts, ...)
{
    va_list args;
    va_start(args, nts);
    char aBuff[K_PRINT_PREALLOC_SIZE];
    ssize_t n = k_print_VaList(pAlloc, pFile, aBuff, sizeof(aBuff), K_NTS(nts), &args);
    va_end(args);
    return n;
}

static ssize_t
formatInteger(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg, bool bUnsigned)
{
    int64_t i = (int64_t)arg;
    uint64_t u = (uint64_t)arg;
    int64_t originalI = i;

    if (pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_ARG_IS_FMT)
        return k_print_eatFmtArg(pFmtArgs, bUnsigned ? (ssize_t)u : i);

    char aBuff[128];
    ssize_t nWritten = 0;

    const char* ntsCharSet = "0123456789abcdef";
    if (i < 0) i = -i;

#define _ITOA_(num, BASE)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if (nWritten >= (ssize_t)sizeof(aBuff))                                                                        \
            break;                                                                                                     \
        aBuff[nWritten++] = ntsCharSet[num % BASE];                                                                    \
        num /= BASE;                                                                                                   \
    } while (num > 0)

    if (bUnsigned)
    {
        if (pFmtArgs->eBase == K_PRINT_BASE_TEN) { _ITOA_(u, 10); }
        else if (pFmtArgs->eBase == K_PRINT_BASE_SIXTEEN) { _ITOA_(u, 16); }
        else if (pFmtArgs->eBase == K_PRINT_BASE_TWO) { _ITOA_(u, 2); }
        else if (pFmtArgs->eBase == K_PRINT_BASE_EIGHT) { _ITOA_(u, 8); }
    }
    else
    {
        if (pFmtArgs->eBase == K_PRINT_BASE_TEN) { _ITOA_(i, 10); }
        else if (pFmtArgs->eBase == K_PRINT_BASE_SIXTEEN) { _ITOA_(i, 16); }
        else if (pFmtArgs->eBase == K_PRINT_BASE_TWO) { _ITOA_(i, 2); }
        else if (pFmtArgs->eBase == K_PRINT_BASE_EIGHT) { _ITOA_(i, 8); }
    }
#undef _ITOA_

    if (pFmtArgs->eBase != K_PRINT_BASE_TEN && pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_HASH)
    {
        if (nWritten + 2 <= (ssize_t)sizeof(aBuff))
        {
            if (pFmtArgs->eBase == K_PRINT_BASE_SIXTEEN)
            {
                aBuff[nWritten++] = 'x';
                aBuff[nWritten++] = '0';
            }
            else if (pFmtArgs->eBase == K_PRINT_BASE_TWO)
            {
                aBuff[nWritten++] = 'b';
                aBuff[nWritten++] = '0';
            }
            else if (pFmtArgs->eBase == K_PRINT_BASE_EIGHT)
            {
                aBuff[nWritten++] = 'o';
            }
        }
    }

    if (!bUnsigned && (pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_SHOW_SIGN || originalI < 0) && nWritten < (ssize_t)sizeof(aBuff))
    {
        if (bUnsigned || originalI >= 0) aBuff[nWritten++] = '+';
        else aBuff[nWritten++] = '-';
    }

    for (ssize_t j = 0; j < nWritten >> 1; ++j)
        K_SWAP(aBuff[j], aBuff[nWritten - 1 - j]);

    ssize_t nn = k_print_BuilderPushSvPaddedFmtArgs(
        pCtx->pBuilder,
        pFmtArgs,
        (k_StringView){aBuff, nWritten}
    );

    return nn;
}

ssize_t
k_print_formatBool(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    bool b = (size_t)arg & 1;
    static const char* map[] = {"false", "true"};
    return k_print_formatNts(pCtx, pFmtArgs, (const void*)map[b]);
}

ssize_t
k_print_formatChar(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    const int64_t c = (int64_t)arg;

    if (pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_ARG_IS_FMT)
        return k_print_eatFmtArg(pFmtArgs, c);

    ssize_t nWritten = 0;
    char toPrint;
    if (c >= 32 && c <= 126) toPrint = c;
    else toPrint = ' ';

    ssize_t n = k_print_BuilderPushSvPaddedFmtArgs(pCtx->pBuilder, pFmtArgs, (k_StringView){&toPrint, 1});
    if (n <= -1) return 0;
    else nWritten += n;

    return nWritten;
}

ssize_t
k_print_formatWChar(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    wchar_t wc = (wchar_t)(uint64_t)arg;
    char aBuff[8] = {0};
    ssize_t nn = wctomb(aBuff, wc);
    return k_print_formatPStringView(pCtx, pFmtArgs, &(k_StringView){aBuff, nn});
}

ssize_t
k_print_formatInt(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    int64_t i = (int)((int64_t)arg);
    return formatInteger(pCtx, pFmtArgs, (void*)i, false);
}

ssize_t
k_print_formatI8(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    int64_t i = (int8_t)((int64_t)arg);
    return formatInteger(pCtx, pFmtArgs, (void*)i, false);
}

ssize_t
k_print_formatU8(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    uint64_t u = (uint8_t)((uint64_t)arg);
    return formatInteger(pCtx, pFmtArgs, (void*)u, true);
}

ssize_t
k_print_formatI16(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    int64_t i = (int16_t)((int64_t)arg);
    return formatInteger(pCtx, pFmtArgs, (void*)i, false);
}

ssize_t
k_print_formatU16(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    uint64_t u = (uint16_t)((uint64_t)arg);
    return formatInteger(pCtx, pFmtArgs, (void*)u, true);
}

ssize_t
k_print_formatI32(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    int64_t i = (int32_t)((int64_t)arg);
    return formatInteger(pCtx, pFmtArgs, (void*)i, false);
}

ssize_t
k_print_formatU32(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    uint64_t u = (uint32_t)((uint64_t)arg);
    return formatInteger(pCtx, pFmtArgs, (void*)u, true);
}

ssize_t
k_print_formatI64(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    int64_t i = (int64_t)arg;
    return formatInteger(pCtx, pFmtArgs, (void*)i, false);
}

ssize_t
k_print_formatU64(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    uint64_t u = (uint64_t)arg;
    return formatInteger(pCtx, pFmtArgs, (void*)u, true);
}

ssize_t
k_print_formatDouble(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    const double d = *(double*)arg;

    if (pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_ARG_IS_FMT)
        return k_print_eatFmtArg(pFmtArgs, (ssize_t)d);

    char aBuff[128];
    ssize_t off = 0;
    ssize_t n = 0;

    if (pFmtArgs->eFmtFlags & K_PRINT_FMT_FLAGS_SHOW_SIGN && d >= 0)
        aBuff[off++] = '+';

    if (pFmtArgs->maxFloatLen < 0) n = d2s_buffered_n(d, aBuff + off);
    else n = d2fixed_buffered_n(d, pFmtArgs->maxFloatLen, aBuff + off);

    ssize_t nPushed = k_print_BuilderPushSvPaddedFmtArgs(
        pCtx->pBuilder,
        pFmtArgs,
        (k_StringView){aBuff, off + n}
    );

    if (nPushed > 0) return nPushed;
    else return 0;
}

ssize_t
k_print_formatPStringView(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    k_StringView sv = *(k_StringView*)arg;
    ssize_t nWritten = k_print_BuilderPushSvPaddedFmtArgs(
        pCtx->pBuilder, pFmtArgs, sv
    );
    if (nWritten <= -1) return 0;
    else return nWritten;
}

ssize_t
k_print_formatPString(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    k_StringView sv = k_StringToSv((k_String*)arg);
    return k_print_formatPStringView(pCtx, pFmtArgs, &sv);
}

ssize_t
k_print_formatNts(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    char* nts = (char*)arg;
    const ssize_t size = strlen(nts);
    return k_print_formatPStringView(pCtx, pFmtArgs, &(k_StringView){nts, size});
}

ssize_t
k_print_formatPtr(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    uint64_t v = (uint64_t)arg;
    pFmtArgs->eFmtFlags |= K_PRINT_FMT_FLAGS_HASH;
    pFmtArgs->eBase = K_PRINT_BASE_SIXTEEN;
    return formatInteger(pCtx, pFmtArgs, (void*)v, true);
}

ssize_t
k_print_formatPBool(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    bool b = *(bool*)arg;
    return k_print_formatBool(pCtx, pFmtArgs, (void*)(uint64_t)b);
}

ssize_t
k_print_formatPChar(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    char c = *(char*)arg;
    return k_print_formatChar(pCtx, pFmtArgs, (void*)(uint64_t)c);
}

ssize_t
k_print_formatPWChar(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    wchar_t wc = *(wchar_t*)arg;
    return k_print_formatWChar(pCtx, pFmtArgs, (void*)(uint64_t)wc);
}

ssize_t
k_print_formatPInt(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    int64_t i = *(int*)arg;
    return formatInteger(pCtx, pFmtArgs, (void*)i, false);
}

ssize_t
k_print_formatPI8(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    int8_t i = *(int8_t*)arg;
    return k_print_formatI8(pCtx, pFmtArgs, (void*)(uint64_t)i);
}

ssize_t
k_print_formatPU8(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    uint8_t i = *(uint8_t*)arg;
    return k_print_formatU8(pCtx, pFmtArgs, (void*)(uint64_t)i);
}

ssize_t
k_print_formatPI16(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    int16_t i = *(int16_t*)arg;
    return k_print_formatI16(pCtx, pFmtArgs, (void*)(uint64_t)i);
}

ssize_t
k_print_formatPU16(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    uint16_t i = *(uint16_t*)arg;
    return k_print_formatU16(pCtx, pFmtArgs, (void*)(uint64_t)i);
}

ssize_t
k_print_formatPI32(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    return k_print_formatPInt(pCtx, pFmtArgs, arg);
}

ssize_t
k_print_formatPU32(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    uint32_t u = *(uint32_t*)arg;
    return k_print_formatU32(pCtx, pFmtArgs, (void*)(uint64_t)u);
}

ssize_t
k_print_formatPI64(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    int64_t u = *(int64_t*)arg;
    return k_print_formatI64(pCtx, pFmtArgs, (void*)(uint64_t)u);
}

ssize_t
k_print_formatPU64(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    uint64_t u = *(uint64_t*)arg;
    return k_print_formatU64(pCtx, pFmtArgs, (void*)(uint64_t)u);
}

ssize_t
k_print_formatPDouble(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    return k_print_formatDouble(pCtx, pFmtArgs, arg);
}

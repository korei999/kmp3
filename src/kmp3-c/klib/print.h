#pragma once

#include "IAllocator.h"
#include "String.h"

#include <stdarg.h>
#include <stdio.h>

typedef uint8_t K_PRINT_BASE;
#define K_PRINT_BASE_TWO 2u
#define K_PRINT_BASE_EIGHT 8u
#define K_PRINT_BASE_TEN 10u
#define K_PRINT_BASE_SIXTEEN 16u

typedef uint16_t K_PRINT_FMT_FLAGS;
#define K_PRINT_FMT_FLAGS_HASH 1u
#define K_PRINT_FMT_FLAGS_SHOW_SIGN (1u << 1)
#define K_PRINT_FMT_FLAGS_ARG_IS_FMT (1u << 2)
#define K_PRINT_FMT_FLAGS_ARG_IS_FLOAT_PRECISION (1u << 3)
#define K_PRINT_FMT_FLAGS_ARG_IS_FILLER (1u << 4)
#define K_PRINT_FMT_FLAGS_JUSTIFY_LEFT (1u << 5)
#define K_PRINT_FMT_FLAGS_JUSTIFY_RIGHT (1u << 6)
#define K_PRINT_FMT_FLAGS_ARG_IS_RANGE (1u << 7)
#define K_PRINT_FMT_FLAGS_ARG_IS_MEMBER_SIZE (1u << 8)

static const ssize_t K_PRINT_FMT_ARG_EATEN = -666999;

#define K_PRINT_PREALLOC_SIZE 256

typedef struct k_print_Map k_print_Map;
typedef struct k_print_Context k_print_Context;
typedef struct k_print_FmtArgs k_print_FmtArgs;

k_print_FmtArgs k_print_FmtArgsCreate(void);
ssize_t k_print_eatFmtArg(k_print_FmtArgs* pFmtArgs, int64_t num); /* Returns K_PRINT_FMT_ARG_EATEN. Used to set FmtArgs with real arg instead of printing the actual arg. */

typedef ssize_t (*k_print_PfnFormat)(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);

k_print_Map* k_print_MapAlloc(k_IAllocator* pAlloc);
void k_print_MapSetGlobal(k_print_Map* pPrinter);
k_print_Map* k_print_MapInst(void); /* Global printer instance. */
bool k_print_MapAddFormatter(k_print_Map* pSelf, const char* ntsSignature, k_print_PfnFormat pfnFormat); /* NOTE: thread unsafe. */
bool k_print_MapAddFormatterSv(k_print_Map* pSelf, const k_StringView svSignature, k_print_PfnFormat pfnFormat); /* NOTE: thread unsafe. */
bool k_print_MapRemoveFormatter(k_print_Map* pSelf, const char* ntsSignature);
bool k_print_MapRemoveFormatterSv(k_print_Map* pSelf, const k_StringView svSignature);
void k_print_MapDestroy(k_print_Map* pSelf);
void k_print_MapDealloc(k_print_Map** pSelf);

struct k_print_FmtArgs
{
    ssize_t maxLen;
    ssize_t padSize;
    ssize_t maxFloatLen;
    ssize_t memberSize;
    ssize_t arraySize;

    K_PRINT_FMT_FLAGS eFmtFlags;
    K_PRINT_BASE eBase;
    char filler;
};

typedef struct
{
    k_IAllocator* pAlloc;
    char* pData;
    ssize_t size;
    ssize_t cap;
    bool bAllocated;
} k_print_Builder;

struct k_print_Context
{
    k_print_Map* pPrinter;
    k_print_Builder* pBuilder;
    k_StringView svFmt;
    ssize_t fmtI;
};

typedef struct
{
    k_IAllocator* pAllocOrNull; /* If null pBuffer of bufferSize will be used. Otherwise builder will be filled using this allocator. */
    char* pBufferOrNull;
    ssize_t preallocOrBufferSize;
} k_print_BuilderInitOpts;

typedef struct
{
    ssize_t size;
    bool bJustifyRight;
    char filler;
} k_print_paddingOpts;

typedef struct
{
    ssize_t off;
    ssize_t size;
} k_print_BuilderSV;

bool k_print_BuilderInit(k_print_Builder* pSelf, k_print_BuilderInitOpts opts);
static inline k_StringView k_print_BuilderToSv(k_print_Builder* s) { return (k_StringView){.pData = s->pData, .size = s->size}; }
static inline k_StringView k_print_BuilderSvToSv(const k_print_Builder* s, k_print_BuilderSV sv);
static inline void k_print_BuilderReset(k_print_Builder* pSelf) { pSelf->size = 0; }
void k_print_BuilderDestroy(k_print_Builder* pSelf);
ssize_t k_print_BuilderPushSvPadded(k_print_Builder* pSelf, const k_StringView sv, k_print_paddingOpts opts);
ssize_t k_print_BuilderPushSvPaddedFmtArgs(k_print_Builder* pSelf, k_print_FmtArgs* pFmtArgs, const k_StringView sv);
ssize_t k_print_BuilderPush(k_print_Builder* pSelf, const char* pStr, ssize_t size);
ssize_t k_print_BuilderPushSv(k_print_Builder* pSelf, const k_StringView sv);
ssize_t k_print_BuilderPushChar(k_print_Builder* pSelf, const char c);

void k_print_BuilderFlush(k_print_Builder* pSelf, FILE* pFile);
k_print_BuilderSV k_print_BuilderPrintVaList(k_print_Builder* pSelf, k_print_FmtArgs* pFmtArgs, const k_StringView svFmt, va_list* pArgs);
k_print_BuilderSV k_print_BuilderPrintSv(k_print_Builder* pSelf, const k_StringView svFmt, ...);
k_print_BuilderSV k_print_BuilderPrint(k_print_Builder* pSelf, const char* ntsFmt, ...);
k_print_BuilderSV k_print_BuilderPrintFmtArgs(k_print_Builder* pSelf, k_print_FmtArgs* pFmtArgs, const char* ntsFmt, ...);
k_print_BuilderSV k_print_BuilderPrintSvFmtArgs(k_print_Builder* pSelf, k_print_FmtArgs* pFmtArgs, k_StringView svFmt, ...);

ssize_t k_print_toBufferVaList(char* pBuff, ssize_t bufferSize, const k_StringView svFmt, va_list* pArgs);
ssize_t k_print_toBufferSv(char* pBuff, ssize_t bufferSize, const k_StringView svFmt, ...);
ssize_t k_print_toBuffer(char* pBuff, ssize_t bufferSize, const char* ntsFmt, ...);

ssize_t k_print_VaList(k_IAllocator* pAlloc, FILE* pFile, char* pBuff, ssize_t bufferSize, const k_StringView svFmt, va_list* pArgs);
ssize_t k_print_Sv(k_IAllocator* pAlloc, FILE* pFile, const k_StringView svFmt, ...);
ssize_t k_print(k_IAllocator* pAlloc, FILE* pFile, const char* nts, ...);

ssize_t k_print_formatBool(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatChar(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatWChar(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatInt(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatI8(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatU8(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatI16(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatU16(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatI32(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatU32(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatI64(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatU64(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatDouble(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPStringView(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPString(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatNts(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPtr(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);

ssize_t k_print_formatPBool(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPChar(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPWChar(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPI8(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPU8(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPI16(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPU16(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPI32(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPU32(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPI64(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPU64(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPInt(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);
ssize_t k_print_formatPDouble(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);

static inline void
k_print_MapAddDefaultFormatters(k_print_Map* s)
{
    /* Larger than 8 bytes structs should be passed by pointer, doubles/floats are handled specially. */
    k_print_MapAddFormatter(s, "bool", k_print_formatBool);
    k_print_MapAddFormatter(s, "b", k_print_formatBool);
    k_print_MapAddFormatter(s, "char", k_print_formatChar);
    k_print_MapAddFormatter(s, "c", k_print_formatChar);
    k_print_MapAddFormatter(s, "wchar_t", k_print_formatWChar);
    k_print_MapAddFormatter(s, "wc", k_print_formatWChar);
    k_print_MapAddFormatter(s, "int", k_print_formatInt);
    k_print_MapAddFormatter(s, "i", k_print_formatI32);
    k_print_MapAddFormatter(s, "u", k_print_formatU32);
    k_print_MapAddFormatter(s, "i8", k_print_formatI8);
    k_print_MapAddFormatter(s, "u8", k_print_formatU8);
    k_print_MapAddFormatter(s, "i16", k_print_formatI16);
    k_print_MapAddFormatter(s, "u16", k_print_formatU16);
    k_print_MapAddFormatter(s, "i32", k_print_formatI32);
    k_print_MapAddFormatter(s, "u32", k_print_formatU32);
    k_print_MapAddFormatter(s, "i64", k_print_formatI64);
    k_print_MapAddFormatter(s, "u64", k_print_formatU64);
    k_print_MapAddFormatter(s, "ssize_t", k_print_formatI64);
    k_print_MapAddFormatter(s, "sz", k_print_formatI64);
    k_print_MapAddFormatter(s, "size_t", k_print_formatU64);
    k_print_MapAddFormatter(s, "uz", k_print_formatU64);
    k_print_MapAddFormatter(s, "float", k_print_formatDouble);
    k_print_MapAddFormatter(s, "f", k_print_formatDouble);
    k_print_MapAddFormatter(s, "double", k_print_formatDouble);
    k_print_MapAddFormatter(s, "d", k_print_formatDouble);
    k_print_MapAddFormatter(s, "PSv", k_print_formatPStringView);
    k_print_MapAddFormatter(s, "PS", k_print_formatPString);
    k_print_MapAddFormatter(s, "nts", k_print_formatNts);
    k_print_MapAddFormatter(s, "s", k_print_formatNts);
    k_print_MapAddFormatter(s, "p", k_print_formatPtr);

    k_print_MapAddFormatter(s, "PBool", k_print_formatPBool);
    k_print_MapAddFormatter(s, "PChar", k_print_formatPChar);
    k_print_MapAddFormatter(s, "PWChar", k_print_formatPWChar);
    k_print_MapAddFormatter(s, "PI8", k_print_formatPI8);
    k_print_MapAddFormatter(s, "PU8", k_print_formatPU8);
    k_print_MapAddFormatter(s, "PI16", k_print_formatPI16);
    k_print_MapAddFormatter(s, "PU16", k_print_formatPU16);
    k_print_MapAddFormatter(s, "PI32", k_print_formatPI32);
    k_print_MapAddFormatter(s, "PU32", k_print_formatPU32);
    k_print_MapAddFormatter(s, "PI64", k_print_formatPI64);
    k_print_MapAddFormatter(s, "PU64", k_print_formatPU64);
    k_print_MapAddFormatter(s, "PInt", k_print_formatPInt);
    k_print_MapAddFormatter(s, "PDouble", k_print_formatPDouble);
}

static inline k_StringView
k_print_BuilderSvToSv(const k_print_Builder* s, k_print_BuilderSV sv)
{
    return (k_StringView){.pData = s->pData + sv.off, .size = sv.size};
}

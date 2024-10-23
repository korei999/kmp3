#pragma once

#include "String.hh"
#include "utils.hh"

#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cuchar>
#include <type_traits>

namespace adt
{
namespace print
{

enum BASE : u8 { TWO = 2, EIGHT = 8, TEN = 10, SIXTEEN = 16 };

struct FormatArgs
{
    u16 maxLen = NPOS16;
    u8 maxFloatLen = NPOS8;
    BASE eBase = BASE::TEN;
    bool bHash = false;
    bool bAlwaysShowSign = false;
};

struct Context
{
    String fmt {};
    char* const pBuff {};
    const u32 buffSize {};
    u32 buffIdx {};
    u32 fmtIdx {};
};

template<typename... ARGS_T> constexpr u32 cout(const String fmt, const ARGS_T&... tArgs);
template<typename... ARGS_T> constexpr u32 cerr(const String fmt, const ARGS_T&... tArgs);

constexpr u32
printArgs(Context ctx)
{
    u32 nRead = 0;
    for (u32 i = ctx.fmtIdx; i < ctx.fmt.size; ++i, ++nRead)
    {
        if (ctx.buffIdx >= ctx.buffSize) break;
        ctx.pBuff[ctx.buffIdx++] = ctx.fmt[i];
    }

    return nRead;
}

inline u32
parseFormatArg(FormatArgs* pArgs, const String fmt, u32 fmtIdx)
{
    u32 nRead = 1;
    bool bDone = false;
    bool bColon = false;
    bool bFloatPresicion = false;
    bool bHash = false;
    bool bHex = false;
    bool bBinary = false;
    bool bAlwaysShowSign = false;

    char buff[32] {};

    for (u32 i = fmtIdx + 1; i < fmt.size; ++i, ++nRead)
    {
        if (bDone) break;

        if (bHash)
        {
            bHash =  false;
            pArgs->eBase = BASE::SIXTEEN;
            pArgs->bHash = true;
        }

        else if (bHex)
        {
            bHex = false;
            pArgs->eBase = BASE::SIXTEEN;
        }
        else if (bBinary)
        {
            bBinary = false;
            pArgs->eBase = BASE::TWO;
        }
        else if (bAlwaysShowSign)
        {
            bAlwaysShowSign = false;
            pArgs->bAlwaysShowSign = true;
        }
        else if (bFloatPresicion)
        {
            bFloatPresicion = false;
            u32 bIdx = 0;
            while (bIdx < sizeof(buff) - 1 && i < fmt.size && fmt[i] != '}')
            {
                buff[bIdx++] = fmt[i++];
                ++nRead;
            }

            pArgs->maxFloatLen = atoi(buff);
        }

        if (bColon)
        {
            if (fmt[i] == '.')
            {
                bFloatPresicion = true;
                continue;
            }
            else if (fmt[i] == '#')
            {
                bHash = true;
                continue;
            }
            else if (fmt[i] == 'x')
            {
                bHex = true;
                continue;
            }
            else if (fmt[i] == 'b')
            {
                bBinary = true;
                continue;
            }
            else if (fmt[i] == '+')
            {
                bAlwaysShowSign = true;
                continue;
            }
        }

        if (fmt[i] == '}')
            bDone = true;
        else if (fmt[i] == ':')
            bColon = true;
    }

    return nRead;
}

template<typename INT_T> requires std::is_integral_v<INT_T>
constexpr char*
intToBuffer(INT_T x, char* pDst, u32 dstSize, FormatArgs fmtArgs)
{
    bool bNegative = false;

    u32 i = 0;
    auto push = [&](char c) -> bool {
        if (i < dstSize)
        {
            pDst[i++] = c;
            return true;
        }

        return false;
    };
 
    if (x == 0) {
        push('0');
        return pDst;
    }
 
    if (x < 0 && fmtArgs.eBase != 10)
    {
        x = -x;
    }
    else if (x < 0 && fmtArgs.eBase == 10)
    {
        bNegative = true;
        x = -x;
    }
 
    while (x != 0)
    {
        int rem = x % fmtArgs.eBase;
        char c = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        push(c);
        x = x / fmtArgs.eBase;
    }
 
    if (fmtArgs.bAlwaysShowSign)
    {
        if (bNegative)
            push('-');
        else push('+');
    }
    else if (bNegative) push('-');

    if (fmtArgs.bHash)
    {
        if (fmtArgs.eBase == BASE::SIXTEEN)
        {
            push('x');
            push('0');
        }
        else if (fmtArgs.eBase == BASE::TWO)
        {
            push('b');
            push('0');
        }
    }

    utils::reverse(pDst, i);
 
    return pDst;
}

constexpr u32
copyBackToBuffer(Context ctx, char* pSrc, u32 srcSize)
{
    u32 i = 0;
    for (; pSrc[i] != '\0' && i < srcSize && ctx.buffIdx < ctx.buffSize; ++i)
        ctx.pBuff[ctx.buffIdx++] = pSrc[i];

    return i;
}

constexpr u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const String& str)
{
    auto& pBuff = ctx.pBuff;
    auto& buffSize = ctx.buffSize;
    auto& buffIdx = ctx.buffIdx;

    u32 nRead = 0;
    for (u32 i = 0; i < str.size && buffIdx < buffSize; ++i, ++nRead)
        pBuff[buffIdx++] = str[i];

    return nRead;
}

constexpr u32
formatToContext(Context ctx, FormatArgs fmtArgs, const char* str)
{
    return formatToContext(ctx, fmtArgs, String(str));
}

constexpr u32
formatToContext(Context ctx, FormatArgs fmtArgs, char* const& pNullTerm)
{
    return formatToContext(ctx, fmtArgs, String(pNullTerm));
}

constexpr u32
formatToContext(Context ctx, FormatArgs fmtArgs, bool b)
{
    return formatToContext(ctx, fmtArgs, b ? "true" : "false");
}

template<typename INT_T> requires std::is_integral_v<INT_T>
constexpr u32
formatToContext(Context ctx, FormatArgs fmtArgs, const INT_T& x)
{
    char buff[32] {};
    char* p = intToBuffer(x, buff, sizeof(buff), fmtArgs);

    return copyBackToBuffer(ctx, p, sizeof(buff));
}

inline u32
formatToContext(Context ctx, FormatArgs fmtArgs, const f32 x)
{
    char nbuff[32] {};
    snprintf(nbuff, sizeof(nbuff), "%.*f", fmtArgs.maxFloatLen, x);

    return copyBackToBuffer(ctx, nbuff, sizeof(nbuff));
}

inline u32
formatToContext(Context ctx, FormatArgs fmtArgs, const f64 x)
{
    char nbuff[32] {};
    snprintf(nbuff, sizeof(nbuff), "%.*lf", fmtArgs.maxFloatLen, x);

    return copyBackToBuffer(ctx, nbuff, sizeof(nbuff));
}

inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const wchar_t x)
{
    char nbuff[4] {};
    snprintf(nbuff, sizeof(nbuff), "%lc", x);

    return copyBackToBuffer(ctx, nbuff, sizeof(nbuff));
}

inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const char32_t x)
{
    char nbuff[MB_LEN_MAX] {};
    mbstate_t ps {};
    c32rtomb(nbuff, x, &ps);

    return copyBackToBuffer(ctx, nbuff, sizeof(nbuff));
}

inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const char x)
{
    char nbuff[4] {};
    snprintf(nbuff, sizeof(nbuff), "%c", x);

    return copyBackToBuffer(ctx, nbuff, sizeof(nbuff));
}

inline u32
formatToContext(Context ctx, FormatArgs fmtArgs, [[maybe_unused]] null nullPtr)
{
    return formatToContext(ctx, fmtArgs, String("nullptr"));
}

template<typename PTR_T> requires std::is_pointer_v<PTR_T>
inline u32
formatToContext(Context ctx, FormatArgs fmtArgs, PTR_T p)
{
    fmtArgs.bHash = true;
    fmtArgs.eBase = BASE::SIXTEEN;
    return formatToContext(ctx, fmtArgs, u64(p));
}

template<typename T, typename... ARGS_T>
constexpr u32
printArgs(Context ctx, const T& tFirst, const ARGS_T&... tArgs)
{
    auto& pBuff = ctx.pBuff;
    auto& buffSize = ctx.buffSize;
    auto& buffIdx = ctx.buffIdx;
    auto& fmt = ctx.fmt;
    auto& fmtIdx = ctx.fmtIdx;

    if (fmtIdx >= fmt.size) return 0;

    u32 nRead = 0;
    bool bArg = false;

    u32 i = fmtIdx;
    for (; i < fmt.size; ++i, ++nRead)
    {
        if (buffIdx >= buffSize) return nRead;

        FormatArgs fmtArgs {};

        if (fmt[i] == '{')
        {
            if (i + 1 < fmt.size && fmt[i + 1] == '{')
            {
                i += 1, nRead += 1;
                bArg = false;
            }
            else bArg = true;
        }

        if (bArg)
        {
            u32 add = parseFormatArg(&fmtArgs, fmt, i);
            u32 addBuff = formatToContext(ctx, fmtArgs, tFirst);
            buffIdx += addBuff;
            i += add;
            nRead += addBuff;
            break;
        }
        else pBuff[buffIdx++] = fmt[i];
    }

    fmtIdx = i;
    nRead += printArgs(ctx, tArgs...);

    return nRead;
}

template<typename... ARGS_T>
constexpr u32
toFILE(FILE* fp, const String fmt, const ARGS_T&... tArgs)
{
    /* TODO: set size / allow allocation maybe */
    char aBuff[1024] {};
    Context ctx {fmt, aBuff, utils::size(aBuff) - 1};
    auto r = printArgs(ctx, tArgs...);
    fputs(aBuff, fp);
    return r;
}

template<typename... ARGS_T>
constexpr u32
toBuffer(char* pBuff, u32 buffSize, const String fmt, const ARGS_T&... tArgs)
{
    Context ctx {fmt, pBuff, buffSize};
    return printArgs(ctx, tArgs...);
}

template<typename... ARGS_T>
constexpr u32
toString(String* pDest, const String fmt, const ARGS_T&... tArgs)
{
    return toBuffer(pDest->pData, pDest->size, fmt, tArgs...);
}

template<typename... ARGS_T>
constexpr u32
cout(const String fmt, const ARGS_T&... tArgs)
{
    return toFILE(stdout, fmt, tArgs...);
}

template<typename... ARGS_T>
constexpr u32
cerr(const String fmt, const ARGS_T&... tArgs)
{
    return toFILE(stderr, fmt, tArgs...);
}

} /* namespace print */
} /* namespace adt */

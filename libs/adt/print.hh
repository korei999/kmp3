#pragma once

#include "String.hh"
#include "utils.hh"

#include <ctype.h> /* win32 */

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cuchar>

#include <type_traits>

namespace adt
{
namespace print
{

enum class BASE : u8 { TWO = 2, EIGHT = 8, TEN = 10, SIXTEEN = 16 };

struct FormatArgs
{
    u16 maxLen = NPOS16;
    u8 maxFloatLen = NPOS8;
    BASE eBase = BASE::TEN;
    bool bHash = false;
    bool bAlwaysShowSign = false;
    bool bArgIsFmt = false;
};

/* TODO: implement reallocatable backing buffer */
struct Context
{
    String fmt {};
    char* const pBuff {};
    const ssize buffSize {};
    ssize buffIdx {};
    ssize fmtIdx {};
    FormatArgs prevFmtArgs {};
    bool bUpdateFmtArgs {};
};

template<typename... ARGS_T> constexpr ssize out(const String fmt, const ARGS_T&... tArgs);
template<typename... ARGS_T> constexpr ssize err(const String fmt, const ARGS_T&... tArgs);

constexpr ssize
printArgs(Context ctx)
{
    ssize nRead = 0;
    for (ssize i = ctx.fmtIdx; i < ctx.fmt.getSize(); ++i, ++nRead)
    {
        if (ctx.buffIdx >= ctx.buffSize) break;
        ctx.pBuff[ctx.buffIdx++] = ctx.fmt[i];
    }

    return nRead;
}

constexpr bool
oneOfChars(const char x, const String chars)
{
    for (auto ch : chars)
        if (ch == x) return true;

    return false;
}

inline ssize
parseFormatArg(FormatArgs* pArgs, const String fmt, ssize fmtIdx)
{
    ssize nRead = 1;
    bool bDone = false;
    bool bColon = false;
    bool bFloatPresicion = false;
    bool bHash = false;
    bool bHex = false;
    bool bBinary = false;
    bool bAlwaysShowSign = false;

    char aBuff[64] {};
    ssize i = fmtIdx + 1;

    auto skipUntil = [&](const String chars) -> void {
        memset(aBuff, 0, sizeof(aBuff));
        ssize bIdx = 0;
        while (bIdx < (ssize)sizeof(aBuff) - 1 && i < fmt.getSize() && !oneOfChars(fmt[i], chars))
        {
            aBuff[bIdx++] = fmt[i++];
            ++nRead;
        }
    };

    auto peek = [&] {
        if (i + 1 < fmt.getSize()) return fmt[i + 1];
        else return '\0';
    };

    for (; i < fmt.getSize(); ++i, ++nRead)
    {
        if (bDone) break;

        if (bHash)
        {
            bHash =  false;
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
            skipUntil("}");
            pArgs->maxFloatLen = atoi(aBuff);
        }

        if (bColon)
        {
            if (fmt[i] == '{')
            {
                skipUntil("}");
                pArgs->bArgIsFmt = true;
                continue;
            }
            else if (fmt[i] == '.')
            {
                if (peek() == '{')
                {
                    skipUntil("}");
                    pArgs->bArgIsFmt = true;
                }

                bFloatPresicion = true;
                continue;
            }
            else if (isdigit(fmt[i]))
            {
                skipUntil("}.#xb");
                pArgs->maxLen = atoi(aBuff);
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
intToBuffer(INT_T x, char* pDst, ssize dstSize, FormatArgs fmtArgs)
{
    bool bNegative = false;

    ssize i = 0;
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
 
    if (x < 0 && int(fmtArgs.eBase) != 10)
    {
        x = -x;
    }
    else if (x < 0 && int(fmtArgs.eBase) == 10)
    {
        bNegative = true;
        x = -x;
    }
 
    while (x != 0)
    {
        int rem = x % int(fmtArgs.eBase);
        char c = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        push(c);
        x = x / int(fmtArgs.eBase);
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

constexpr ssize
copyBackToBuffer(Context ctx, char* pSrc, ssize srcSize)
{
    ssize i = 0;
    for (; pSrc[i] != '\0' && i < srcSize && ctx.buffIdx < ctx.buffSize; ++i)
        ctx.pBuff[ctx.buffIdx++] = pSrc[i];

    return i;
}

constexpr ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const String& str)
{
    auto& pBuff = ctx.pBuff;
    auto& buffSize = ctx.buffSize;
    auto& buffIdx = ctx.buffIdx;

    ssize nRead = 0;
    for (ssize i = 0; buffIdx < buffSize; ++i, ++nRead)
    {
        if (i < str.getSize())
            pBuff[buffIdx++] = str[i];
        else if (i < fmtArgs.maxLen && fmtArgs.maxLen != NPOS16) /* fill extra space */
            pBuff[buffIdx++] = ' ';
        else break;
    }

    return nRead;
}

constexpr ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const char* str)
{
    return formatToContext(ctx, fmtArgs, String(str));
}

constexpr ssize
formatToContext(Context ctx, FormatArgs fmtArgs, char* const& pNullTerm)
{
    return formatToContext(ctx, fmtArgs, String(pNullTerm));
}

constexpr ssize
formatToContext(Context ctx, FormatArgs fmtArgs, bool b)
{
    return formatToContext(ctx, fmtArgs, b ? "true" : "false");
}

template<typename INT_T> requires std::is_integral_v<INT_T>
constexpr ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const INT_T& x)
{
    char buff[64] {};
    char* p = intToBuffer(x, buff, utils::size(buff), fmtArgs);
    if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen < utils::size(buff) - 1)
        buff[fmtArgs.maxLen] = '\0';

    return copyBackToBuffer(ctx, p, utils::size(buff));
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const f32 x)
{
    char aBuff[64] {};
    if (fmtArgs.maxFloatLen == NPOS8)
        snprintf(aBuff, utils::size(aBuff), "%g", x);
    else snprintf(aBuff, utils::size(aBuff), "%.*f", fmtArgs.maxFloatLen, x);

    return copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const f64 x)
{
#if defined __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

    char aBuff[128] {};
    if (fmtArgs.maxFloatLen == NPOS8)
        snprintf(aBuff, utils::size(aBuff), "%g", x);
    else snprintf(aBuff, utils::size(aBuff), "%.*lf", fmtArgs.maxFloatLen, x);

#if defined __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#pragma GCC diagnostic pop
#endif

    return copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const wchar_t x)
{
    char aBuff[4] {};
#ifdef _WIN32
    snprintf(aBuff, utils::size(aBuff), "%lc", (wint_t)x);
#else
    snprintf(aBuff, utils::size(aBuff), "%lc", x);
#endif

    return copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const char32_t x)
{
    char aBuff[MB_LEN_MAX] {};
    mbstate_t ps {};
    c32rtomb(aBuff, x, &ps);

    return copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const char x)
{
    char aBuff[4] {};
    snprintf(aBuff, utils::size(aBuff), "%c", x);

    return copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, [[maybe_unused]] null nullPtr)
{
    return formatToContext(ctx, fmtArgs, String("nullptr"));
}

template<typename PTR_T> requires std::is_pointer_v<PTR_T>
inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, PTR_T p)
{
    if (p == nullptr) return formatToContext(ctx, fmtArgs, nullptr);

    fmtArgs.bHash = true;
    fmtArgs.eBase = BASE::SIXTEEN;
    return formatToContext(ctx, fmtArgs, usize(p));
}

template<typename T, typename... ARGS_T>
constexpr ssize
printArgs(Context ctx, const T& tFirst, const ARGS_T&... tArgs)
{
    ssize nRead = 0;
    bool bArg = false;
    ssize i = ctx.fmtIdx;

    /* NOTE: ugly edge case, when we need to fill with spaces but fmt is out of range */
    if (ctx.bUpdateFmtArgs && ctx.fmtIdx >= ctx.fmt.getSize())
        return formatToContext(ctx, ctx.prevFmtArgs, tFirst);
    else if (ctx.fmtIdx >= ctx.fmt.getSize())
        return 0;

    for (; i < ctx.fmt.getSize(); ++i, ++nRead)
    {
        if (ctx.buffIdx >= ctx.buffSize) return nRead;

        FormatArgs fmtArgs {};

        if (ctx.bUpdateFmtArgs)
        {
            ctx.bUpdateFmtArgs = false;

            fmtArgs = ctx.prevFmtArgs;
            ssize addBuff = formatToContext(ctx, fmtArgs, tFirst);

            ctx.buffIdx += addBuff;
            nRead += addBuff;

            break;
        }
        else if (ctx.fmt[i] == '{')
        {
            if (i + 1 < ctx.fmt.getSize() && ctx.fmt[i + 1] == '{')
            {
                i += 1, nRead += 1;
                bArg = false;
            }
            else bArg = true;
        }

        if (bArg)
        {
            ssize addBuff = 0;
            ssize add = parseFormatArg(&fmtArgs, ctx.fmt, i);

            if (fmtArgs.bArgIsFmt)
            {
                if constexpr (std::is_integral_v<std::remove_reference_t<decltype(tFirst)>>)
                {
                    /* FIXME: these two should be separate */
                    fmtArgs.maxLen = tFirst;
                    fmtArgs.maxFloatLen = tFirst;

                    ctx.prevFmtArgs = fmtArgs;
                    ctx.bUpdateFmtArgs = true;
                }
            }
            else addBuff = formatToContext(ctx, fmtArgs, tFirst);

            ctx.buffIdx += addBuff;
            i += add;
            nRead += addBuff;

            break;
        }
        else ctx.pBuff[ctx.buffIdx++] = ctx.fmt[i];
    }

    ctx.fmtIdx = i;
    nRead += printArgs(ctx, tArgs...);

    return nRead;
}

template<ssize SIZE = 512, typename... ARGS_T>
constexpr ssize
toFILE(FILE* fp, const String fmt, const ARGS_T&... tArgs)
{
    /* TODO: allow allocation? */
    char aBuff[SIZE] {};
    Context ctx {fmt, aBuff, utils::size(aBuff) - 1};
    auto r = printArgs(ctx, tArgs...);
    fputs(aBuff, fp);
    return r;
}

template<typename... ARGS_T>
constexpr ssize
toBuffer(char* pBuff, ssize buffSize, const String fmt, const ARGS_T&... tArgs)
{
    Context ctx {fmt, pBuff, buffSize};
    return printArgs(ctx, tArgs...);
}

template<typename... ARGS_T>
constexpr ssize
toString(String* pDest, const String fmt, const ARGS_T&... tArgs)
{
    return toBuffer(pDest->data(), pDest->getSize(), fmt, tArgs...);
}

template<typename... ARGS_T>
constexpr ssize
out(const String fmt, const ARGS_T&... tArgs)
{
    return toFILE(stdout, fmt, tArgs...);
}

template<typename... ARGS_T>
constexpr ssize
err(const String fmt, const ARGS_T&... tArgs)
{
    return toFILE(stderr, fmt, tArgs...);
}

} /* namespace print */
} /* namespace adt */

#pragma once

#include "String.hh"
#include "utils.hh"
#include "Span.hh"

#include <ctype.h> /* win32 */

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cuchar>

#include <type_traits>

namespace adt::print
{

enum class BASE : u8 { TWO = 2, EIGHT = 8, TEN = 10, SIXTEEN = 16 };
enum class JUSTIFY : u8 { RIGHT = '>', LEFT = '<' };

struct FormatArgs
{
    u16 maxLen = NPOS16;
    u8 maxFloatLen = NPOS8;
    BASE eBase = BASE::TEN;
    bool bHash = false;
    bool bAlwaysShowSign = false;
    bool bArgIsFmt = false;
    JUSTIFY eJustify = JUSTIFY::LEFT;
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

template<typename... ARGS_T> inline ssize out(const String fmt, const ARGS_T&... tArgs) noexcept;
template<typename... ARGS_T> inline ssize err(const String fmt, const ARGS_T&... tArgs) noexcept;

inline ssize
printArgs(Context ctx) noexcept
{
    ssize nRead = 0;
    for (ssize i = ctx.fmtIdx; i < ctx.fmt.getSize(); ++i, ++nRead)
    {
        if (ctx.buffIdx >= ctx.buffSize) break;
        ctx.pBuff[ctx.buffIdx++] = ctx.fmt[i];
    }

    return nRead;
}

inline constexpr bool
oneOfChars(const char x, const String chars) noexcept
{
    for (auto ch : chars)
        if (ch == x) return true;

    return false;
}

inline ssize
parseFormatArg(FormatArgs* pArgs, const String fmt, ssize fmtIdx) noexcept
{
    ssize nRead = 1;
    bool bDone = false;
    bool bColon = false;
    bool bFloatPresicion = false;
    bool bHash = false;
    bool bHex = false;
    bool bBinary = false;
    bool bAlwaysShowSign = false;
    bool bRightJustify = false;

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
        else if (bRightJustify)
        {
            bRightJustify = false;
            pArgs->eJustify = JUSTIFY::RIGHT;
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
            else if (fmt[i] == '>')
            {
                bRightJustify = true;
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
inline constexpr char*
intToBuffer(INT_T x, char* pDst, ssize dstSize, FormatArgs fmtArgs) noexcept
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

inline ssize
copyBackToBuffer(Context ctx, FormatArgs fmtArgs, const Span<char> spSrc) noexcept
{
    ssize i = 0;

    auto copySpan = [&]
    {
        for (; i < spSrc.getSize() && spSrc[i] && ctx.buffIdx < ctx.buffSize; ++i)
            ctx.pBuff[ctx.buffIdx++] = spSrc[i];
    };

    if (fmtArgs.eJustify == JUSTIFY::LEFT)
    {
        copySpan();

        if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen > i)
        {
            for (; ctx.buffIdx < ctx.buffSize && i < fmtArgs.maxLen; ++i)
                ctx.pBuff[ctx.buffIdx++] = ' ';
        }
    }
    else
    {
        /* leave space for the string */
        ssize nSpaces = fmtArgs.maxLen - strnlen(spSrc.data(), spSrc.getSize());
        ssize j = 0;

        if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen > i && nSpaces > 0)
        {
            for (j = 0; ctx.buffIdx < ctx.buffSize && j < nSpaces; ++j)
                ctx.pBuff[ctx.buffIdx++] = ' ';
        }

        copySpan();

        i += j;
    }

    return i;
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const String& str) noexcept
{
    return copyBackToBuffer(ctx, fmtArgs, {const_cast<char*>(str.data()), str.getSize()});
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const char* str) noexcept
{
    return formatToContext(ctx, fmtArgs, String(str));
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, char* const& pNullTerm) noexcept
{
    return formatToContext(ctx, fmtArgs, String(pNullTerm));
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, bool b) noexcept
{
    return formatToContext(ctx, fmtArgs, b ? "true" : "false");
}

template<typename INT_T> requires std::is_integral_v<INT_T>
inline constexpr ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const INT_T& x) noexcept
{
    char buff[64] {};
    intToBuffer(x, buff, utils::size(buff), fmtArgs);
    if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen < utils::size(buff) - 1)
        buff[fmtArgs.maxLen] = '\0';

    return copyBackToBuffer(ctx, fmtArgs, {buff});
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const f32 x) noexcept
{
    char aBuff[64] {};
    if (fmtArgs.maxFloatLen == NPOS8)
        snprintf(aBuff, utils::size(aBuff), "%g", x);
    else snprintf(aBuff, utils::size(aBuff), "%.*f", fmtArgs.maxFloatLen, x);

    return copyBackToBuffer(ctx, fmtArgs, {aBuff});
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const f64 x) noexcept
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

    return copyBackToBuffer(ctx, fmtArgs, {aBuff});
}

inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const wchar_t x) noexcept
{
    char aBuff[4] {};
#ifdef _WIN32
    snprintf(aBuff, utils::size(aBuff), "%lc", (wint_t)x);
#else
    snprintf(aBuff, utils::size(aBuff), "%lc", x);
#endif

    return copyBackToBuffer(ctx, fmtArgs, {aBuff});
}

inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const char32_t x) noexcept
{
    char aBuff[MB_LEN_MAX] {};
    mbstate_t ps {};
    c32rtomb(aBuff, x, &ps);

    return copyBackToBuffer(ctx, fmtArgs, {aBuff});
}

inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const char x) noexcept
{
    char aBuff[4] {};
    snprintf(aBuff, utils::size(aBuff), "%c", x);

    return copyBackToBuffer(ctx, fmtArgs, {aBuff});
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, [[maybe_unused]] null nullPtr) noexcept
{
    return formatToContext(ctx, fmtArgs, String("nullptr"));
}

template<typename PTR_T> requires std::is_pointer_v<PTR_T>
inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, PTR_T p) noexcept
{
    if (p == nullptr) return formatToContext(ctx, fmtArgs, nullptr);

    fmtArgs.bHash = true;
    fmtArgs.eBase = BASE::SIXTEEN;
    return formatToContext(ctx, fmtArgs, usize(p));
}

template<typename T, typename... ARGS_T>
inline constexpr ssize
printArgs(Context ctx, const T& tFirst, const ARGS_T&... tArgs) noexcept
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
inline ssize
toFILE(FILE* fp, const String fmt, const ARGS_T&... tArgs) noexcept
{
    /* TODO: allow allocation? */
    char aBuff[SIZE] {};
    Context ctx {fmt, aBuff, utils::size(aBuff) - 1};
    auto r = printArgs(ctx, tArgs...);
    fputs(aBuff, fp);
    return r;
}

template<typename... ARGS_T>
inline constexpr ssize
toBuffer(char* pBuff, ssize buffSize, const String fmt, const ARGS_T&... tArgs) noexcept
{
    Context ctx {fmt, pBuff, buffSize};
    return printArgs(ctx, tArgs...);
}

template<typename... ARGS_T>
inline constexpr ssize
toString(String* pDest, const String fmt, const ARGS_T&... tArgs) noexcept
{
    return toBuffer(pDest->data(), pDest->getSize(), fmt, tArgs...);
}

template<typename... ARGS_T>
inline ssize
out(const String fmt, const ARGS_T&... tArgs) noexcept
{
    return toFILE(stdout, fmt, tArgs...);
}

template<typename... ARGS_T>
inline ssize
err(const String fmt, const ARGS_T&... tArgs) noexcept
{
    return toFILE(stderr, fmt, tArgs...);
}

} /* namespace adt::print */

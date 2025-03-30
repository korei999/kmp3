#pragma once

#include "String.hh"
#include "utils.hh"
#include "Span.hh"
#include "enum.hh"

#include <ctype.h> /* win32 */

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cuchar> /* IWYU pragma: keep */

namespace adt::print
{

enum class BASE : u8 { TWO = 2, EIGHT = 8, TEN = 10, SIXTEEN = 16 };

enum class FMT_FLAGS : u8
{
    HASH = 1,
    ALWAYS_SHOW_SIGN = 1 << 1,
    ARG_IS_FMT = 1 << 2,
    FLOAT_PRECISION_ARG = 1 << 3,
    JUSTIFY_RIGHT = 1 << 4,
};
ADT_ENUM_BITWISE_OPERATORS(FMT_FLAGS);

struct FormatArgs
{
    u16 maxLen = NPOS16;
    u8 maxFloatLen = NPOS8;
    BASE eBase = BASE::TEN;
    FMT_FLAGS eFmtFlags {};
};

struct Context
{
    StringView fmt {};
    char* const pBuff {};
    const ssize buffSize {};
    ssize buffIdx {};
    ssize fmtIdx {};
    FormatArgs prevFmtArgs {};
    bool bUpdateFmtArgs {};
};

template<typename ...ARGS_T> inline ssize out(const StringView fmt, const ARGS_T&... tArgs) noexcept;
template<typename ...ARGS_T> inline ssize err(const StringView fmt, const ARGS_T&... tArgs) noexcept;

inline ssize
printArgs(Context ctx) noexcept
{
    ssize nRead = 0;
    for (ssize i = ctx.fmtIdx; i < ctx.fmt.size(); ++i, ++nRead)
    {
        if (ctx.buffIdx >= ctx.buffSize) break;
        ctx.pBuff[ctx.buffIdx++] = ctx.fmt[i];
    }

    return nRead;
}

inline constexpr bool
oneOfChars(const char x, const StringView chars) noexcept
{
    for (auto ch : chars)
        if (ch == x) return true;

    return false;
}

inline ssize
parseFormatArg(FormatArgs* pArgs, const StringView fmt, ssize fmtIdx) noexcept
{
    ssize nRead = 1;
    bool bDone = false;
    bool bColon = false;
    bool bFloatPresicion = false;

    char aBuff[64] {};
    ssize i = fmtIdx + 1;

    auto clSkipUntil = [&](const StringView chars) -> void
    {
        memset(aBuff, 0, sizeof(aBuff));
        ssize bIdx = 0;
        while (bIdx < (ssize)sizeof(aBuff) - 1 && i < fmt.size() && !oneOfChars(fmt[i], chars))
        {
            aBuff[bIdx++] = fmt[i++];
            ++nRead;
        }
    };

    auto clPeek = [&]
    {
        if (i + 1 < fmt.size())
            return fmt[i + 1];
        else return '\0';
    };

    for (; i < fmt.size(); ++i, ++nRead)
    {
        if (bDone) break;

        if (bFloatPresicion)
        {
            clSkipUntil("}");
            pArgs->maxFloatLen = atoi(aBuff);
            pArgs->eFmtFlags |= FMT_FLAGS::FLOAT_PRECISION_ARG;
        }

        if (bColon)
        {
            if (fmt[i] == '{')
            {
                clSkipUntil("}");
                pArgs->eFmtFlags |= FMT_FLAGS::ARG_IS_FMT;
                continue;
            }
            else if (fmt[i] == '.')
            {
                if (clPeek() == '{')
                {
                    clSkipUntil("}");
                    pArgs->eFmtFlags |= FMT_FLAGS::ARG_IS_FMT;
                }

                bFloatPresicion = true;
                continue;
            }
            else if (isdigit(fmt[i]))
            {
                clSkipUntil("}.#xb");
                pArgs->maxLen = atoi(aBuff);
            }
            else if (fmt[i] == '#')
            {
                pArgs->eFmtFlags |= FMT_FLAGS::HASH;
                continue;
            }
            else if (fmt[i] == 'x')
            {
                pArgs->eBase = BASE::SIXTEEN;
                continue;
            }
            else if (fmt[i] == 'b')
            {
                pArgs->eBase = BASE::TWO;
                continue;
            }
            else if (fmt[i] == '+')
            {
                pArgs->eFmtFlags |= FMT_FLAGS::ALWAYS_SHOW_SIGN;
                continue;
            }
            else if (fmt[i] == '>')
            {
                pArgs->eFmtFlags |= FMT_FLAGS::JUSTIFY_RIGHT;
                continue;
            }
        }

        if (fmt[i] == '}') bDone = true;
        else if (fmt[i] == ':') bColon = true;
    }

    return nRead;
}

template<typename INT_T> requires std::is_integral_v<INT_T>
inline constexpr void
intToBuffer(INT_T x, Span<char> spBuff, FormatArgs fmtArgs) noexcept
{
    bool bNegative = false;

    ssize i = 0;
    auto clPush = [&](char c) -> bool
    {
        if (i < spBuff.size())
        {
            spBuff[i++] = c;
            return true;
        }

        return false;
    };
 
    if (x == 0)
    {
        clPush('0');
        return;
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
        clPush(c);
        x = x / int(fmtArgs.eBase);
    }
 
    if (bool(fmtArgs.eFmtFlags & FMT_FLAGS::ALWAYS_SHOW_SIGN))
    {
        if (bNegative)
            clPush('-');
        else clPush('+');
    }
    else if (bNegative)
    {
        clPush('-');
    }

    if (bool(fmtArgs.eFmtFlags & FMT_FLAGS::HASH))
    {
        if (fmtArgs.eBase == BASE::SIXTEEN)
        {
            clPush('x');
            clPush('0');
        }
        else if (fmtArgs.eBase == BASE::TWO)
        {
            clPush('b');
            clPush('0');
        }
    }

    utils::reverse(spBuff.data(), i);
}

inline ssize
copyBackToContext(Context ctx, FormatArgs fmtArgs, const Span<char> spSrc) noexcept
{
    if (spSrc.empty()) return 0;

    ssize i = 0;

    auto clCopySpan = [&]
    {
        for (; i < spSrc.size() && spSrc[i] && ctx.buffIdx < ctx.buffSize; ++i)
            ctx.pBuff[ctx.buffIdx++] = spSrc[i];
    };

    if (bool(fmtArgs.eFmtFlags & FMT_FLAGS::JUSTIFY_RIGHT))
    {
        /* leave space for the string */
        ssize nSpaces = fmtArgs.maxLen - strnlen(spSrc.data(), spSrc.size());
        ssize j = 0;

        if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen > i && nSpaces > 0)
        {
            for (j = 0; ctx.buffIdx < ctx.buffSize && j < nSpaces; ++j)
                ctx.pBuff[ctx.buffIdx++] = ' ';
        }

        clCopySpan();

        i += j;
    }
    else
    {
        clCopySpan();

        if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen > i)
        {
            for (; ctx.buffIdx < ctx.buffSize && i < fmtArgs.maxLen; ++i)
                ctx.pBuff[ctx.buffIdx++] = ' ';
        }
    }

    return i;
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const StringView& str) noexcept
{
    return copyBackToContext(ctx, fmtArgs, {const_cast<char*>(str.data()), str.size()});
}

template<int SIZE> requires(SIZE > 1)
inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const StringFixed<SIZE>& str) noexcept
{
    return copyBackToContext(ctx, fmtArgs, {const_cast<char*>(str.data()), str.size()});
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const char* str) noexcept
{
    return formatToContext(ctx, fmtArgs, StringView(str));
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, char* const& pNullTerm) noexcept
{
    return formatToContext(ctx, fmtArgs, StringView(pNullTerm));
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
    intToBuffer(x, {buff}, fmtArgs);
    if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen < utils::size(buff) - 1)
        buff[fmtArgs.maxLen] = '\0';

    return copyBackToContext(ctx, fmtArgs, {buff});
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const f32 x) noexcept
{
    char aBuff[64] {};
    if (fmtArgs.maxFloatLen == NPOS8)
        snprintf(aBuff, utils::size(aBuff), "%g", x);
    else
        snprintf(aBuff, utils::size(aBuff), "%.*f", fmtArgs.maxFloatLen, x);

    return copyBackToContext(ctx, fmtArgs, {aBuff});
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const f64 x) noexcept
{
    char aBuff[128] {};
    if (fmtArgs.maxFloatLen == NPOS8)
        snprintf(aBuff, utils::size(aBuff), "%g", x);
    else
        snprintf(aBuff, utils::size(aBuff), "%.*lf", fmtArgs.maxFloatLen, x);

    return copyBackToContext(ctx, fmtArgs, {aBuff});
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const wchar_t x) noexcept
{
    char aBuff[8] {};
#ifdef _WIN32
    snprintf(aBuff, utils::size(aBuff) - 1, "%lc", (wint_t)x);
#else
    snprintf(aBuff, utils::size(aBuff) - 1, "%lc", x);
#endif

    return copyBackToContext(ctx, fmtArgs, {aBuff});
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const char32_t x) noexcept
{
    return formatToContext(ctx, fmtArgs, (wchar_t)x);
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const char x) noexcept
{
    char aBuff[4] {};
    snprintf(aBuff, utils::size(aBuff), "%c", x);

    return copyBackToContext(ctx, fmtArgs, {aBuff});
}

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, null) noexcept
{
    return formatToContext(ctx, fmtArgs, StringView("nullptr"));
}

template<typename PTR_T> requires std::is_pointer_v<PTR_T>
inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, PTR_T p) noexcept
{
    if (p == nullptr)
        return formatToContext(ctx, fmtArgs, nullptr);

    fmtArgs.eFmtFlags |= FMT_FLAGS::HASH;
    fmtArgs.eBase = BASE::SIXTEEN;
    return formatToContext(ctx, fmtArgs, usize(p));
}

template<typename T, typename ...ARGS_T>
inline constexpr ssize
printArgs(Context ctx, const T& tFirst, const ARGS_T&... tArgs) noexcept
{
    ssize nRead = 0;
    bool bArg = false;
    ssize i = ctx.fmtIdx;

    /* NOTE: ugly edge case, when we need to fill with spaces but fmt is out of range */
    if (ctx.bUpdateFmtArgs && ctx.fmtIdx >= ctx.fmt.size())
        return formatToContext(ctx, ctx.prevFmtArgs, tFirst);
    else if (ctx.fmtIdx >= ctx.fmt.size())
        return 0;

    for (; i < ctx.fmt.size(); ++i, ++nRead)
    {
        if (ctx.buffIdx >= ctx.buffSize)
            return nRead;

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
            if (i + 1 < ctx.fmt.size() && ctx.fmt[i + 1] == '{')
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

            if (bool(fmtArgs.eFmtFlags & FMT_FLAGS::ARG_IS_FMT))
            {
                if constexpr (std::is_integral_v<std::remove_reference_t<decltype(tFirst)>>)
                {
                    if (bool(fmtArgs.eFmtFlags & FMT_FLAGS::FLOAT_PRECISION_ARG))
                        fmtArgs.maxFloatLen = tFirst;
                    else fmtArgs.maxLen = tFirst;

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

template<ssize SIZE = 512, typename ...ARGS_T>
inline ssize
toFILE(FILE* fp, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    char aBuff[SIZE] {};
    Context ctx {fmt, aBuff, utils::size(aBuff) - 1};
    auto r = printArgs(ctx, tArgs...);
    fputs(aBuff, fp);
    return r;
}

template<typename ...ARGS_T>
inline constexpr ssize
toBuffer(char* pBuff, ssize buffSize, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    if (!pBuff || buffSize <= 0)
        return 0;

    Context ctx {fmt, pBuff, buffSize};
    return printArgs(ctx, tArgs...);
}

template<typename ...ARGS_T>
inline constexpr ssize
toString(StringView* pDest, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    return toBuffer(pDest->data(), pDest->size(), fmt, tArgs...);
}

template<typename ...ARGS_T>
inline constexpr ssize
toSpan(Span<char> sp, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    /* leave 1 byte for '\0' */
    return toBuffer(sp.data(), sp.size() - 1, fmt, tArgs...);
}

template<typename ...ARGS_T>
inline ssize
out(const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    return toFILE(stdout, fmt, tArgs...);
}

template<typename ...ARGS_T>
inline ssize
err(const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    return toFILE(stderr, fmt, tArgs...);
}

inline ssize
FormatArgsToFmt(const FormatArgs fmtArgs, Span<char> spFmt) noexcept
{
    ssize i = 0;
    auto clPush = [&](char c) -> bool
    {
        if (i < spFmt.size())
        {
            spFmt[i++] = c;

            return true;
        }

        return false;
    };

    if (!clPush('{')) return i;

    if (fmtArgs.maxLen != NPOS16 || fmtArgs.maxFloatLen != NPOS8)
    {
        if (!clPush(':')) return i;

        if (bool(fmtArgs.eFmtFlags & FMT_FLAGS::JUSTIFY_RIGHT))
            if (!clPush('>')) return i;

        if (fmtArgs.maxFloatLen != NPOS8)
            if (!clPush('.')) return i;

        if (bool(fmtArgs.eFmtFlags & FMT_FLAGS::ARG_IS_FMT))
        {
            if (!clPush('{')) return i;
            if (!clPush('}')) return i;
        }
        else
        {
            char aBuff[64] {};
            if (fmtArgs.maxFloatLen != NPOS8)
                intToBuffer(fmtArgs.maxFloatLen, {aBuff}, {});
            else
                intToBuffer(fmtArgs.maxLen, {aBuff}, {});

            for (ssize j = 0; j < utils::size(aBuff) && aBuff[j]; ++j)
                if (!clPush(aBuff[j])) return i;
        }
    }

    if (!clPush('}')) return i;

    return i;
}

template<template<typename> typename CON_T, typename T>
inline ssize
formatToContextExpSize(Context ctx, FormatArgs fmtArgs, const CON_T<T>& x, const ssize contSize) noexcept
{
    if (contSize <= 0)
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "(empty)");
    }

    char aFmtBuff[64] {};
    ssize nFmtRead = FormatArgsToFmt(fmtArgs, {aFmtBuff, sizeof(aFmtBuff) - 2});

    StringView sFmtArg = aFmtBuff;
    aFmtBuff[nFmtRead++] = ',';
    aFmtBuff[nFmtRead++] = ' ';
    StringView sFmtArgComma(aFmtBuff);

    char aBuff[1024] {};
    ssize nRead = 0;
    ssize i = 0;

    for (const auto& e : x)
    {
        const StringView fmt = i == contSize - 1 ? sFmtArg : sFmtArgComma;
        nRead += toBuffer(aBuff + nRead, utils::size(aBuff) - nRead, fmt, e);
        ++i;
    }

    return copyBackToContext(ctx, fmtArgs, {aBuff});
}

template<template<typename, ssize> typename CON_T, typename T, ssize SIZE>
inline ssize
formatToContextTemplateSize(Context ctx, FormatArgs fmtArgs, const CON_T<T, SIZE>& x, const ssize contSize) noexcept
{
    if (contSize <= 0)
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "(empty)");
    }

    char aFmtBuff[64] {};
    ssize nFmtRead = FormatArgsToFmt(fmtArgs, {aFmtBuff, sizeof(aFmtBuff) - 2});

    StringView sFmtArg = aFmtBuff;
    aFmtBuff[nFmtRead++] = ',';
    aFmtBuff[nFmtRead++] = ' ';
    StringView sFmtArgComma(aFmtBuff);

    char aBuff[1024] {};
    ssize nRead = 0;
    ssize i = 0;

    for (const auto& e : x)
    {
        const StringView fmt = i == contSize - 1 ? sFmtArg : sFmtArgComma;
        nRead += toBuffer(aBuff + nRead, utils::size(aBuff) - nRead, fmt, e);
        ++i;
    }

    return copyBackToContext(ctx, fmtArgs, {aBuff});
}

template<template<typename> typename CON_T, typename T>
inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const CON_T<T>& x) noexcept
{
    return formatToContextExpSize(ctx, fmtArgs, x, x.size());
}

template<typename T, ssize N>
inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const T (&a)[N]) noexcept
{
    return formatToContext(ctx, fmtArgs, Span(a, N));
}

} /* namespace adt::print */

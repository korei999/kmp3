#pragma once

#include "print.inc"

#include "String.hh" /* IWYU pragma: keep */
#include "enum.hh"

#include <ctype.h> /* win32 */
#include <cstdio>
#include <cstdlib>
#include <cuchar> /* IWYU pragma: keep */

#if __has_include(<unistd.h>)

    #include <unistd.h>

#elif defined _WIN32

    #include <direct.h>
    #define getcwd _getcwd

#endif

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
    const isize buffSize {};
    isize buffIdx {};
    isize fmtIdx {};
    FormatArgs prevFmtArgs {};
    bool bUpdateFmtArgs {};
};

inline const char*
_currentWorkingDirectory()
{
    static char aBuff[300] {};
    return getcwd(aBuff, sizeof(aBuff) - 1);
}

inline const char*
stripSourcePath(const char* ntsSourcePath)
{
    static const StringView svCwd = _currentWorkingDirectory();
    return ntsSourcePath + svCwd.size() + 1;
}

inline isize
printArgs(Context ctx) noexcept
{
    isize nRead = 0;
    for (isize i = ctx.fmtIdx; i < ctx.fmt.size(); ++i, ++nRead)
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

inline isize
parseFormatArg(FormatArgs* pArgs, const StringView fmt, isize fmtIdx) noexcept
{
    isize nRead = 1;
    bool bDone = false;
    bool bColon = false;
    bool bFloatPresicion = false;

    char aBuff[64] {};
    isize i = fmtIdx + 1;

    auto clSkipUntil = [&](const StringView chars) -> void
    {
        memset(aBuff, 0, sizeof(aBuff));
        isize bIdx = 0;
        while (bIdx < (isize)sizeof(aBuff) - 1 && i < fmt.size() && !oneOfChars(fmt[i], chars))
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

template<typename INT_T>
requires std::is_integral_v<INT_T>
inline constexpr void
intToBuffer(INT_T x, Span<char> spBuff, FormatArgs fmtArgs) noexcept
{
    bool bNegative = false;

    isize i = 0;
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

inline isize
copyBackToContext(Context ctx, FormatArgs fmtArgs, const Span<char> spSrc) noexcept
{
    isize i = 0;

    auto clCopySpan = [&]
    {
        for (; i < spSrc.size() && spSrc[i] && ctx.buffIdx < ctx.buffSize && i < fmtArgs.maxLen; ++i)
            ctx.pBuff[ctx.buffIdx++] = spSrc[i];
    };

    if (bool(fmtArgs.eFmtFlags & FMT_FLAGS::JUSTIFY_RIGHT))
    {
        /* leave space for the string */
        isize nSpaces = fmtArgs.maxLen - strnlen(spSrc.data(), spSrc.size());
        isize j = 0;

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

inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, const StringView str) noexcept
{
    return copyBackToContext(ctx, fmtArgs, {const_cast<char*>(str.data()), str.size()});
}

template<typename STRING_T>
requires ConvertsToStringView<STRING_T>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const STRING_T& str) noexcept
{
    return copyBackToContext(ctx, fmtArgs, {const_cast<char*>(str.data()), isize(str.size())});
}

inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, const char* str) noexcept
{
    return formatToContext(ctx, fmtArgs, StringView(str));
}

inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, char* const& pNullTerm) noexcept
{
    return formatToContext(ctx, fmtArgs, StringView(pNullTerm));
}

inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, bool b) noexcept
{
    return formatToContext(ctx, fmtArgs, b ? "true" : "false");
}

template<typename INT_T>
requires std::is_integral_v<INT_T>
inline constexpr isize
formatToContext(Context ctx, FormatArgs fmtArgs, const INT_T& x) noexcept
{
    char buff[64] {};
    intToBuffer(x, {buff}, fmtArgs);
    if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen < utils::size(buff) - 1)
        buff[fmtArgs.maxLen] = '\0';

    return copyBackToContext(ctx, fmtArgs, {buff});
}

inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, const f32 x) noexcept
{
    char aBuff[64] {};
    if (fmtArgs.maxFloatLen == NPOS8)
        snprintf(aBuff, utils::size(aBuff), "%g", x);
    else
        snprintf(aBuff, utils::size(aBuff), "%.*f", fmtArgs.maxFloatLen, x);

    return copyBackToContext(ctx, fmtArgs, {aBuff});
}

inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, const f64 x) noexcept
{
    char aBuff[128] {};
    if (fmtArgs.maxFloatLen == NPOS8)
        snprintf(aBuff, utils::size(aBuff), "%g", x);
    else
        snprintf(aBuff, utils::size(aBuff), "%.*lf", fmtArgs.maxFloatLen, x);

    return copyBackToContext(ctx, fmtArgs, {aBuff});
}

inline isize
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

inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, const char32_t x) noexcept
{
    return formatToContext(ctx, fmtArgs, (wchar_t)x);
}

inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, const char x) noexcept
{
    char aBuff[4] {};
    snprintf(aBuff, utils::size(aBuff), "%c", x);

    return copyBackToContext(ctx, fmtArgs, {aBuff});
}

inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, null) noexcept
{
    return formatToContext(ctx, fmtArgs, StringView("nullptr"));
}

inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, Empty) noexcept
{
    return formatToContext(ctx, fmtArgs, StringView("Empty"));
}

template<typename A, typename B>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const Pair<A, B>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.first, x.second);
}

template<typename PTR_T>
requires std::is_pointer_v<PTR_T>
inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, const PTR_T p) noexcept
{
    if (p == nullptr) return formatToContext(ctx, fmtArgs, nullptr);

    fmtArgs.eFmtFlags |= FMT_FLAGS::HASH;
    fmtArgs.eBase = BASE::SIXTEEN;
    return formatToContext(ctx, fmtArgs, usize(p));
}

template<typename T>
inline constexpr void
_printArgInternal(isize& nRead, isize& i, bool& bArg, Context& ctx, const T& arg) noexcept
{
    for (; i < ctx.fmt.size(); ++i, ++nRead)
    {
        if (ctx.buffIdx >= ctx.buffSize)
            return;

        FormatArgs fmtArgs {};

        if (ctx.bUpdateFmtArgs)
        {
            ctx.bUpdateFmtArgs = false;

            fmtArgs = ctx.prevFmtArgs;
            isize addBuff = formatToContext(ctx, fmtArgs, arg);

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
            isize addBuff = 0;
            isize add = parseFormatArg(&fmtArgs, ctx.fmt, i);

            if (bool(fmtArgs.eFmtFlags & FMT_FLAGS::ARG_IS_FMT))
            {
                if constexpr (std::is_integral_v<std::remove_reference_t<decltype(arg)>>)
                {
                    if (bool(fmtArgs.eFmtFlags & FMT_FLAGS::FLOAT_PRECISION_ARG))
                        fmtArgs.maxFloatLen = arg;
                    else fmtArgs.maxLen = arg;

                    ctx.prevFmtArgs = fmtArgs;
                    ctx.bUpdateFmtArgs = true;
                }
            }
            else addBuff = formatToContext(ctx, fmtArgs, arg);

            ctx.buffIdx += addBuff;
            i += add;
            nRead += addBuff;

            break;
        }
        else ctx.pBuff[ctx.buffIdx++] = ctx.fmt[i];
    }
}

template<typename T, typename ...ARGS_T>
inline constexpr isize
printArgs(Context ctx, const T& tFirst, const ARGS_T&... tArgs) noexcept
{
    isize nRead = 0;
    bool bArg = false;
    isize i = ctx.fmtIdx;

    /* NOTE: ugly edge case, when we need to fill with spaces but fmt is out of range */
    if (ctx.bUpdateFmtArgs && ctx.fmtIdx >= ctx.fmt.size())
        return formatToContext(ctx, ctx.prevFmtArgs, tFirst);
    else if (ctx.fmtIdx >= ctx.fmt.size())
        return 0;

    _printArgInternal(nRead, i, bArg, ctx, tFirst);

    ctx.fmtIdx = i;
    nRead += printArgs(ctx, tArgs...);

    return nRead;
}

template<isize SIZE, typename ...ARGS_T>
inline isize
toFILE(FILE* fp, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    char aBuff[SIZE] {};
    Context ctx {fmt, aBuff, utils::size(aBuff) - 1};
    auto r = printArgs(ctx, tArgs...);
    fputs(aBuff, fp);
    return r;
}

template<typename ...ARGS_T>
inline constexpr isize
toBuffer(char* pBuff, isize buffSize, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    if (!pBuff || buffSize <= 0)
        return 0;

    Context ctx {fmt, pBuff, buffSize};
    return printArgs(ctx, tArgs...);
}

template<typename ...ARGS_T>
inline constexpr isize
toString(StringView* pDest, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    return toBuffer(pDest->data(), pDest->size(), fmt, tArgs...);
}

template<typename ...ARGS_T>
inline constexpr isize
toSpan(Span<char> sp, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    /* leave 1 byte for '\0' */
    return toBuffer(sp.data(), sp.size() - 1, fmt, tArgs...);
}

template<typename ...ARGS_T>
inline isize
out(const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    return toFILE(stdout, fmt, tArgs...);
}

template<typename ...ARGS_T>
inline isize
err(const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    return toFILE(stderr, fmt, tArgs...);
}

inline isize
FormatArgsToFmt(const FormatArgs fmtArgs, Span<char> spFmt) noexcept
{
    isize i = 0;
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

            for (isize j = 0; j < utils::size(aBuff) && aBuff[j]; ++j)
                if (!clPush(aBuff[j])) return i;
        }
    }

    if (!clPush('}')) return i;

    return i;
}

inline isize
formatToContextExpSize(Context ctx, FormatArgs fmtArgs, const auto& x, const isize contSize) noexcept
{
    if (contSize <= 0)
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "[]");
    }

    char aFmtBuff[64] {};
    isize nFmtRead = FormatArgsToFmt(fmtArgs, {aFmtBuff, sizeof(aFmtBuff) - 2});

    StringView sFmtArg = aFmtBuff;
    aFmtBuff[nFmtRead++] = ',';
    aFmtBuff[nFmtRead++] = ' ';
    StringView sFmtArgComma(aFmtBuff);

    char aBuff[512] {};
    aBuff[0] = '[';
    isize nRead = 1;
    isize i = 0;

    for (const auto& e : x)
    {
        const StringView fmt = i == contSize - 1 ? sFmtArg : sFmtArgComma;
        nRead += toBuffer(aBuff + nRead, utils::size(aBuff) - nRead, fmt, e);
        ++i;
    }

    if (nRead < utils::size(aBuff)) aBuff[nRead++] = ']';

    return copyBackToContext(ctx, {}, {aBuff, nRead});
}

inline isize
formatToContextUntilEnd(Context ctx, FormatArgs fmtArgs, const auto& x) noexcept
{
    if (!x.data())
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "[]");
    }

    char aFmtBuff[64] {};
    isize nFmtRead = FormatArgsToFmt(fmtArgs, {aFmtBuff, sizeof(aFmtBuff) - 2});

    StringView sFmtArg = aFmtBuff;
    aFmtBuff[nFmtRead++] = ',';
    aFmtBuff[nFmtRead++] = ' ';
    StringView sFmtArgComma(aFmtBuff);

    char aBuff[512] {};
    aBuff[0] = '[';
    isize nRead = 1;

    for (auto it = x.begin(); it != x.end(); ++it)
    {
        const StringView fmt = it.next() == x.end() ? sFmtArg : sFmtArgComma;
        nRead += toBuffer(aBuff + nRead, utils::size(aBuff) - nRead, fmt, *it);
    }

    if (nRead < utils::size(aBuff)) aBuff[nRead++] = ']';

    return copyBackToContext(ctx, {}, {aBuff, nRead});
}

template<typename T>
requires (HasSizeMethod<T> && !ConvertsToStringView<T>)
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const T& x) noexcept
{
    return formatToContextExpSize(ctx, fmtArgs, x, x.size());
}

template<typename T>
requires HasNextIt<T>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const T& x) noexcept
{
    return print::formatToContextUntilEnd(ctx, fmtArgs, x);
}

template<typename T, isize N>
inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, const T (&a)[N]) noexcept
{
    return formatToContextExpSize(ctx, fmtArgs, a, N);
}

template<typename T>
requires (!Printable<T>)
inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, const T&) noexcept
{
    const StringView sv = typeName<T>();

#if defined __clang__ || __GNUC__

    const StringView svSub = "T = ";
    const isize atI = sv.subStringAt(svSub);
    const StringView svDemangled = [&]
    {
        if (atI != NPOS)
        {
            return StringView {
                const_cast<char*>(sv.data() + atI + svSub.size()),
                    sv.size() - atI - svSub.size() - 1
            };
        }
        else
        {
            return sv;
        }
    }();

#elif defined _WIN32

    const StringView svSub = "typeName<";
    const isize atI = sv.subStringAt(svSub);
    const StringView svDemangled = [&]
    {
        if (atI != NPOS)
        {
            return StringView {
                const_cast<char*>(sv.data() + atI + svSub.size()),
                    sv.size() - atI - svSub.size() - 1
            };
        }
        else
        {
            return sv;
        }
    }();

#else

    const StringView svDemangled = sv;

#endif

    return formatToContext(ctx, fmtArgs, svDemangled);
}

} /* namespace adt::print */

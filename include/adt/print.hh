#pragma once

#include "print.inc"

#include "String.hh" /* IWYU pragma: keep */
#include "defer.hh"

#include <ctype.h> /* win32 */
#include <cstdio>
#include <cuchar> /* IWYU pragma: keep */
#include <charconv>

#if __has_include(<unistd.h>)

    #include <unistd.h>

#elif defined _WIN32

    #include <direct.h>
    #define getcwd _getcwd

#endif

namespace adt::print
{

inline
Buffer::Buffer(IAllocator* pAlloc, isize prealloc)
    : m_pAlloc {pAlloc}
{
    m_pData = pAlloc->mallocV<char>(prealloc);
    m_cap = prealloc;
    m_bDataAllocated = true;
}

inline
Buffer::operator StringView() noexcept
{
    return {m_pData, m_size};
}

inline
Buffer::operator String() noexcept
{
    ADT_ASSERT(m_bDataAllocated && m_pAlloc, "{}, {}", m_bDataAllocated, m_pAlloc);

    String r;
    r.m_pData = m_pData;
    r.m_size = m_size;
    return r;
}

inline isize
Buffer::push(char c)
{
    if (m_size >= m_cap)
    {
        if (!m_pAlloc) return -1;

        grow(utils::max(isize(8), m_cap * 2));
    }

    m_pData[m_size++] = c;
    return m_size - 1;
}

inline isize
Buffer::push(const Span<const char> sp)
{
    if (sp.empty()) return m_size;

    if (m_size + sp.size() > m_cap)
    {
        if (!m_pAlloc) return -1;

        grow(utils::max(isize(8), nextPowerOf2(m_cap + sp.size())));
    }

    ::memcpy(m_pData + m_size, sp.data(), sp.size());
    m_size += sp.size();
    return m_size - sp.size();
}

inline isize
Buffer::push(const StringView sv)
{
    return push(Span{sv.data(), sv.size()});
}

inline isize
Buffer::pushN(const char c, const isize nTimes)
{
    if (nTimes <= 0) return m_size;

    if (m_size + nTimes > m_cap)
    {
        if (!m_pAlloc) return -1;

        grow(utils::max(isize(8), nextPowerOf2(m_cap + nTimes)));
    }

    ::memset(m_pData + m_size, c, nTimes);
    m_size += nTimes;
    return m_size - nTimes;
}

inline void
Buffer::grow(isize newCap)
{
    char* pNewData {};

    if (!m_bDataAllocated)
    {
        pNewData = m_pAlloc->zallocV<char>(newCap);
        m_bDataAllocated = true;
        memcpy(pNewData, m_pData, m_size);
    }
    else
    {
        pNewData = m_pAlloc->relocate(m_pData, m_size, newCap);
    }

    m_cap = newCap;
    m_pData = pNewData;
}

template<typename T>
constexpr const StringView
typeName()
{
#ifdef __clang__
    return __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
    return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
    return __FUNCSIG__;
#else
    return "unsupported compiler";
#endif
}

inline const char*
currentWorkingDirectory()
{
    static char aBuff[300] {};
    return getcwd(aBuff, sizeof(aBuff) - 1);
}

inline const char*
stripSourcePath(const char* ntsSourcePath)
{
    static const StringView svCwd = currentWorkingDirectory();
    return ntsSourcePath + svCwd.size() + 1;
}

inline isize
printArgs(Context ctx)
{
    const StringView svFmtSlice = ctx.fmt.subString(ctx.fmtIdx, ctx.fmt.size() - ctx.fmtIdx);
    if (ctx.pBuffer->push(svFmtSlice) != -1)
        return svFmtSlice.size();

    return 0;
}

inline isize
parseFormatArg(FormatArgs* pArgs, const StringView fmt, isize fmtIdx) noexcept
{
    isize nWritten = 1;
    bool bDone = false;
    bool bColon = false;
    bool bFloatPresicion = false;

    char aBuff[64] {};
    isize buffIdx = 0;
    isize i = fmtIdx + 1;

    auto clSkipUntil = [&](const StringView svCharSet) -> void
    {
        memset(aBuff, 0, sizeof(aBuff));
        buffIdx = 0;
        while (buffIdx < (isize)sizeof(aBuff) - 1 && i < fmt.size() && !svCharSet.contains(fmt[i]))
        {
            aBuff[buffIdx++] = fmt[i++];
            ++nWritten;
        }
    };

    auto clPeek = [&]
    {
        if (i + 1 < fmt.size())
            return fmt[i + 1];
        else return '\0';
    };

    for (; i < fmt.size(); ++i, ++nWritten)
    {
        if (bDone) break;

        if (bFloatPresicion)
        {
            clSkipUntil("}");
            pArgs->maxFloatLen = StringView(aBuff, buffIdx).toI64();
            pArgs->eFmtFlags |= FormatArgs::FLAGS::FLOAT_PRECISION_ARG;
        }

        if (bColon)
        {
            if (fmt[i] == '{')
            {
                clSkipUntil("}");
                pArgs->eFmtFlags |= FormatArgs::FLAGS::ARG_IS_FMT;
                continue;
            }
            else if (fmt[i] == '.')
            {
                if (clPeek() == '{')
                {
                    clSkipUntil("}");
                    pArgs->eFmtFlags |= FormatArgs::FLAGS::ARG_IS_FMT;
                }

                bFloatPresicion = true;
                continue;
            }
            else if (isdigit(fmt[i]))
            {
                if (fmt[i] == '0')
                {
                    if (i + 1 < fmt.size())
                        pArgs->filler = '0';
                }

                clSkipUntil("}.#xb");
                pArgs->maxLen = StringView(aBuff, buffIdx).toI64();
            }
            else if (fmt[i] == '#')
            {
                pArgs->eFmtFlags |= FormatArgs::FLAGS::HASH;
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
            else if (fmt[i] == 'o')
            {
                pArgs->eBase = BASE::EIGHT;
                continue;
            }
            else if (fmt[i] == '+')
            {
                pArgs->eFmtFlags |= FormatArgs::FLAGS::ALWAYS_SHOW_SIGN;
                continue;
            }
            else if (fmt[i] == '>')
            {
                pArgs->eFmtFlags |= FormatArgs::FLAGS::JUSTIFY_RIGHT;
                continue;
            }
        }

        if (fmt[i] == '}') bDone = true;
        else if (fmt[i] == ':') bColon = true;
    }

    return nWritten;
}

template<typename T>
inline isize
intToBuffer(T x, Span<char> spBuff, FormatArgs fmtArgs) noexcept
{
    isize i = 0;
    bool bNegative = false;
    ADT_DEFER( utils::reverse(spBuff.data(), i) );

#define PUSH_OR_RET(x)                                                                                                 \
    if (i < spBuff.size())                                                                                             \
        spBuff[i++] = x;                                                                                               \
    else                                                                                                               \
        return i;
 
    if (x == 0)
    {
        PUSH_OR_RET('0');
        return i;
    }
 
    if (x < 0 && fmtArgs.eBase != BASE::TEN)
    {
        x = -x;
    }
    else if (x < 0 && fmtArgs.eBase == BASE::TEN)
    {
        bNegative = true;
        x = -x;
    }

    const char* ntsCharSet = "0123456789abcdef";

#define _ITOA_(BASE)                                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        PUSH_OR_RET(ntsCharSet[x % BASE]);                                                                             \
    } while ((x /= BASE) > 0);

    /* About 2.5x faster code than generic base. */
    if (fmtArgs.eBase == BASE::TEN) { _ITOA_(10); }
    else if (fmtArgs.eBase == BASE::EIGHT) { _ITOA_(8); }
    else if (fmtArgs.eBase == BASE::SIXTEEN) { _ITOA_(16); }
    else if (fmtArgs.eBase == BASE::TWO) { _ITOA_(2); }

#undef _ITOA_
 
    if (bool(fmtArgs.eFmtFlags & FormatArgs::FLAGS::ALWAYS_SHOW_SIGN))
    {
        if (bNegative)
        {
            PUSH_OR_RET('-');
        }
        else
        {
            PUSH_OR_RET('+');
        }
    }
    else if (bNegative)
    {
        PUSH_OR_RET('-');
    }

    if (bool(fmtArgs.eFmtFlags & FormatArgs::FLAGS::HASH))
    {
        if (fmtArgs.eBase == BASE::SIXTEEN)
        {
            PUSH_OR_RET('x');
            PUSH_OR_RET('0');
        }
        else if (fmtArgs.eBase == BASE::TWO)
        {
            PUSH_OR_RET('b');
            PUSH_OR_RET('0');
        }
        else if (fmtArgs.eBase == BASE::EIGHT)
        {
            PUSH_OR_RET('o');
        }
    }

#undef PUSH_OR_RET

    return i;
}

inline isize
copyBackToContext(Context ctx, FormatArgs fmtArgs, const StringView sv)
{
    isize i = 0;
    const char filler = fmtArgs.filler ? fmtArgs.filler : ' ';

    auto clCopySpan = [&]
    {
        const isize mLen = utils::min(sv.size(), isize(fmtArgs.maxLen));
        if (ctx.pBuffer->push(Span{sv.data(), mLen}) != -1)
            i += mLen;
    };

    if (bool(fmtArgs.eFmtFlags & FormatArgs::FLAGS::JUSTIFY_RIGHT))
    {
        /* leave space for the string */
        const isize nSpaces = fmtArgs.maxLen - sv.size();
        isize j = 0;

        if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen > i && nSpaces > 0)
        {
            if (ctx.pBuffer->pushN(filler, nSpaces) != -1)
                j += nSpaces;
        }

        clCopySpan();

        i += j;
    }
    else
    {
        clCopySpan();

        if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen > i)
        {
            if (ctx.pBuffer->pushN(filler, fmtArgs.maxLen - i) != -1)
                i += fmtArgs.maxLen;
        }
    }

    return i;
}

inline isize
format(Context ctx, FormatArgs fmtArgs, const StringView sv)
{
    return copyBackToContext(ctx, fmtArgs, sv);
}

template<typename STRING_T>
requires ConvertsToStringView<STRING_T>
inline isize format(Context ctx, FormatArgs fmtArgs, const STRING_T& str)
{
    const isize realLen = strnlen(str.data(), str.size());
    return copyBackToContext(ctx, fmtArgs, {const_cast<char*>(str.data()), realLen});
}

inline isize
format(Context ctx, FormatArgs fmtArgs, const char* str)
{
    return format(ctx, fmtArgs, StringView(str));
}

inline isize
format(Context ctx, FormatArgs fmtArgs, char* const& pNullTerm)
{
    return format(ctx, fmtArgs, StringView(pNullTerm));
}

inline isize
format(Context ctx, FormatArgs fmtArgs, bool b)
{
    return format(ctx, fmtArgs, b ? "true" : "false");
}

inline isize
format(Context ctx, FormatArgs fmtArgs, const wchar_t x)
{
    char aBuff[8] {};
#ifdef _WIN32
    const isize n = snprintf(aBuff, utils::size(aBuff) - 1, "%lc", (wint_t)x);
#else
    const isize n = snprintf(aBuff, utils::size(aBuff) - 1, "%lc", x);
#endif

    return copyBackToContext(ctx, fmtArgs, {aBuff, n});
}

inline isize
format(Context ctx, FormatArgs fmtArgs, const char32_t x)
{
    return format(ctx, fmtArgs, (wchar_t)x);
}

inline isize
format(Context ctx, FormatArgs fmtArgs, const char x)
{
    char aBuff[4] {};
    const isize n = snprintf(aBuff, utils::size(aBuff), "%c", x);

    return copyBackToContext(ctx, fmtArgs, {aBuff, n});
}

inline isize
format(Context ctx, FormatArgs fmtArgs, null)
{
    return format(ctx, fmtArgs, StringView("nullptr"));
}

inline isize
format(Context ctx, FormatArgs fmtArgs, Empty)
{
    return format(ctx, fmtArgs, StringView("Empty"));
}

template<typename T>
inline isize
format(Context ctx, FormatArgs fmtArgs, const T* const p)
{
    if (p == nullptr) return format(ctx, fmtArgs, nullptr);

    fmtArgs.eFmtFlags |= FormatArgs::FLAGS::HASH;
    fmtArgs.eBase = BASE::SIXTEEN;
    return format(ctx, fmtArgs, usize(p));
}

namespace details
{

template<typename T>
inline constexpr void
printArg(isize& rNWritten, isize& rI, bool& rbArg, Context& rCtx, const T& rArg)
{
    for (; rI < rCtx.fmt.size(); ++rI, ++rNWritten)
    {
        FormatArgs fmtArgs {};

        if (bool(rCtx.eFlags & Context::FLAGS::UPDATE_FMT_ARGS))
        {
            rCtx.eFlags &= ~Context::FLAGS::UPDATE_FMT_ARGS;

            fmtArgs = rCtx.prevFmtArgs;
            isize addBuff = format(rCtx, fmtArgs, rArg);

            rNWritten += addBuff;

            break;
        }

        const StringView svFmtSlice = rCtx.fmt.subString(rI, rCtx.fmt.size() - rI);
        const isize openBraceI = svFmtSlice.charAt('{');
        const StringView svFmtUntilOpenBrace = svFmtSlice.subString(0, openBraceI == -1 ? svFmtSlice.size() : openBraceI);

        rCtx.pBuffer->push(svFmtUntilOpenBrace);
        rI += svFmtUntilOpenBrace.size();
        rNWritten += svFmtUntilOpenBrace.size();

        /* No '{' case. */
        if (rI >= rCtx.fmt.size()) break;

        rbArg = true;
        if (rI + 1 < rCtx.fmt.size() && rCtx.fmt[rI + 1] == '{')
        {
            rI += 1, rNWritten += 1;
            rbArg = false;
        }

        if (rbArg)
        {
            isize addBuff = 0;
            const isize add = parseFormatArg(&fmtArgs, rCtx.fmt, rI);

            if (bool(fmtArgs.eFmtFlags & FormatArgs::FLAGS::ARG_IS_FMT))
            {
                if constexpr (std::is_integral_v<std::remove_reference_t<decltype(rArg)>>)
                {
                    if (bool(fmtArgs.eFmtFlags & FormatArgs::FLAGS::FLOAT_PRECISION_ARG))
                        fmtArgs.maxFloatLen = rArg;
                    else fmtArgs.maxLen = rArg;

                    rCtx.prevFmtArgs = fmtArgs;
                    rCtx.eFlags |= Context::FLAGS::UPDATE_FMT_ARGS;
                }
            }
            else
            {
                addBuff = format(rCtx, fmtArgs, rArg);
            }

            rI += add;
            rNWritten += addBuff;

            break;
        }
        else
        {
            rCtx.pBuffer->push(rCtx.fmt[rI]);
        }
    }
}

inline isize
formatVariadic(Context, FormatArgs) noexcept
{
    return 0;
}

template<typename T>
inline isize
formatVariadic(Context ctx, FormatArgs fmtArgs, const T& x)
{
    return format(ctx, fmtArgs, x);
}

template<typename T, typename ...ARGS>
inline isize
formatVariadic(Context ctx, FormatArgs fmtArgs, const T& first, const ARGS&... args)
{
    isize n = format(ctx, fmtArgs, first);
    if (n < 0) return n;

    if (ctx.pBuffer->push(StringView{", "}) == -1) return n;
    n += 2;

    return n + details::formatVariadic(ctx, fmtArgs, args...);
}

template<typename T>
inline isize
formatFloat(Context ctx, FormatArgs fmtArgs, const T x)
{
    char aBuff[64] {};
    std::to_chars_result res {};
    if (fmtArgs.maxFloatLen == NPOS8)
        res = std::to_chars(aBuff, aBuff + sizeof(aBuff), x);
    else res = std::to_chars(aBuff, aBuff + sizeof(aBuff), x, std::chars_format::fixed, fmtArgs.maxFloatLen);

    if (res.ptr) return copyBackToContext(ctx, fmtArgs, {aBuff, res.ptr - aBuff});
    else return copyBackToContext(ctx, fmtArgs, {aBuff});
}

} /* namespace details */

template<typename T> requires (std::is_integral_v<T>)
inline constexpr isize
format(Context ctx, FormatArgs fmtArgs, const T x)
{
    char aBuff[64] {};
    const isize n = intToBuffer(x, {aBuff}, fmtArgs);
    if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen < utils::size(aBuff) - 1)
        aBuff[fmtArgs.maxLen] = '\0';

    return copyBackToContext(ctx, fmtArgs, {aBuff, n});
}

inline isize
format(Context ctx, FormatArgs fmtArgs, const f32 x)
{
    return details::formatFloat<f32>(ctx, fmtArgs, x);
}

inline isize
format(Context ctx, FormatArgs fmtArgs, const f64 x)
{
    return details::formatFloat<f64>(ctx, fmtArgs, x);
}

template<typename T, typename ...ARGS_T>
inline constexpr isize
printArgs(Context ctx, const T& tFirst, const ARGS_T&... tArgs)
{
    isize nWritten = 0;
    bool bArg = false;
    isize i = ctx.fmtIdx;

    if (ctx.fmtIdx >= ctx.fmt.size())
    {
        /* NOTE: Edge case, when we need to fill but fmt is out of range. */
        if (bool(ctx.eFlags & Context::FLAGS::UPDATE_FMT_ARGS))
            return format(ctx, ctx.prevFmtArgs, tFirst);
        else return 0;
    }

    details::printArg(nWritten, i, bArg, ctx, tFirst);

    ctx.fmtIdx = i;
    nWritten += printArgs(ctx, tArgs...);

    return nWritten;
}

template<isize SIZE, typename ...ARGS_T>
inline isize
toFILE(FILE* fp, const StringView fmt, const ARGS_T&... tArgs)
{
    return toFILE<SIZE>(nullptr, fp, fmt, tArgs...);
}

template<isize SIZE, typename ...ARGS_T>
inline isize
toFILE(IAllocator* pAlloc, FILE* fp, const StringView fmt, const ARGS_T&... tArgs)
{
    char aPreallocated[SIZE] {};
    Buffer buff {pAlloc, aPreallocated, sizeof(aPreallocated) - 1};

    try
    {
        Context ctx {.fmt = fmt, .pBuffer = &buff};
        const isize r = printArgs(ctx, tArgs...);
        fwrite(ctx.pBuffer->m_pData, r, 1, fp);
    }
    catch (const AllocException& ex)
    {
#ifdef ADT_DBG_MEMORY
        ex.printErrorMsg(stderr);
#endif
    }

    if (pAlloc && buff.m_pData != aPreallocated) pAlloc->free(buff.m_pData);

    return buff.m_size;
}

template<typename ...ARGS_T>
inline constexpr isize
toBuffer(char* pBuff, isize buffSize, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    if (!pBuff || buffSize <= 0) return 0;

    Buffer buff {pBuff, buffSize};

    Context ctx {.fmt = fmt, .pBuffer = &buff};
    printArgs(ctx, tArgs...);

    return buff.m_size;
}

template<typename ...ARGS_T>
inline constexpr isize
toSpan(Span<char> sp, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    /* leave 1 byte for '\0' */
    return toBuffer(sp.data(), sp.size() - 1, fmt, tArgs...);
}

template<typename ...ARGS_T>
inline String
toString(IAllocator* pAlloc, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    Buffer buff {pAlloc};

    Context ctx {.fmt = fmt, .pBuffer = &buff};

    try
    {
        printArgs(ctx, tArgs...);
        buff.push('\0');
        buff.m_size -= 1;
    }
    catch (const AllocException& ex)
    {
#ifdef ADT_DBG_MEMORY
        ex.printErrorMsg(stderr);
#endif
        if (buff.m_size > 0)
        {
            buff.m_pData[buff.m_size] = '\0';
            buff.m_size -= 1;
        }
        else
        {
            return {};
        }
    }

    return String(buff);
}

template<typename ...ARGS_T>
[[nodiscard]] inline String
toString(IAllocator* pAlloc, isize prealloc, const StringView fmt, const ARGS_T&... tArgs) noexcept
{
    Buffer buff;

    try
    {
        new(&buff) Buffer {pAlloc, prealloc};
        Context ctx {.fmt = fmt, .pBuffer = &buff};
        printArgs(ctx, tArgs...);
        buff.push('\0');
        buff.m_size -= 1;
    }
    catch (const AllocException& ex)
    {
#ifdef ADT_DBG_MEMORY
        ex.printErrorMsg(stderr);
#endif
        if (buff.m_size > 0)
        {
            buff.m_pData[buff.m_size] = '\0';
            buff.m_size -= 1;
        }
        else
        {
            return {};
        }
    }

    return String(buff);
}

template<typename ...ARGS_T>
inline isize
out(const StringView fmt, const ARGS_T&... tArgs)
{
    return toFILE(stdout, fmt, tArgs...);
}

template<typename ...ARGS_T>
inline isize
err(const StringView fmt, const ARGS_T&... tArgs)
{
    return toFILE(stderr, fmt, tArgs...);
}

inline isize
formatExpSize(Context ctx, FormatArgs fmtArgs, const auto& x, const isize contSize)
{
    if (contSize <= 0)
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "[]");
    }

    if (ctx.pBuffer->push('[') < 0) return 0;

    isize nWritten = 1;
    isize i = 0;

    for (const auto& e : x)
    {
        const isize n = format(ctx, fmtArgs, e);
        if (n < 0) break;

        nWritten += n;

        if (i < contSize - 1)
        {
            char ntsMore[] = ", ";
            const isize n2 = copyBackToContext(ctx, {}, ntsMore);

            nWritten += n2;
        }

        ++i;
    }

    if (ctx.pBuffer->push(']') >= 0) ++nWritten;

    return nWritten;
}

inline isize
formatUntilEnd(Context ctx, FormatArgs fmtArgs, const auto& x)
{
    if (!x.data())
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "[]");
    }

    if (ctx.pBuffer->push('[') < 0) return 0;
    isize nWritten = 1;

    for (auto it = x.begin(); it != x.end(); ++it)
    {
        const isize n = format(ctx, fmtArgs, *it);
        if (n < 0) break;

        nWritten += n;

        if (it.next() != x.end())
        {
            char ntsMore[] = ", ";
            const isize n2 = copyBackToContext(ctx, {}, ntsMore);

            nWritten += n2;
        }
    }

    if (ctx.pBuffer->push(']') >= 0) ++nWritten;

    return nWritten;
}

template<typename ...ARGS>
inline isize
formatVariadic(Context ctx, FormatArgs fmtArgs, const ARGS&... args)
{
    const bool bSquareBrackets = bool(fmtArgs.eFmtFlags & FormatArgs::FLAGS::SQUARE_BRACKETS);
    isize n = 0;

    if (bSquareBrackets)
    {
        if (ctx.pBuffer->push('[') < 0) return n;
        ++n;
    }

    const isize nFormatted = details::formatVariadic(ctx, fmtArgs, args...);
    if (nFormatted < 0) return n;

    n += nFormatted;

    if (bSquareBrackets)
    {
        if (ctx.pBuffer->push(']') < 0) return n;
        ++n;
    }

    return n;
}

template<typename T>
requires (HasSizeMethod<T> && !ConvertsToStringView<T>)
inline isize
format(Context ctx, FormatArgs fmtArgs, const T& x)
{
    return formatExpSize(ctx, fmtArgs, x, x.size());
}

template<typename T>
requires HasNextIt<T>
inline isize
format(Context ctx, FormatArgs fmtArgs, const T& x)
{
    return print::formatUntilEnd(ctx, fmtArgs, x);
}

template<typename T, isize N>
inline isize
format(Context ctx, FormatArgs fmtArgs, const T (&a)[N])
{
    return formatExpSize(ctx, fmtArgs, a, N);
}

template<typename T>
requires (!Printable<T>)
inline isize
format(Context ctx, FormatArgs fmtArgs, const T&)
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

    return format(ctx, fmtArgs, svDemangled);
}

} /* namespace adt::print */

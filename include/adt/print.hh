#pragma once

#include "print-inl.hh"

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
Builder::Builder(IAllocator* pAlloc, isize prealloc)
    : m_pAlloc {pAlloc}
{
    m_pData = pAlloc->mallocV<char>(prealloc);
    m_cap = prealloc;
    m_bDataAllocated = true;
}

inline
Builder::operator StringView() noexcept
{
    return {m_pData, m_size};
}

inline
Builder::operator String() noexcept
{
    ADT_ASSERT(m_bDataAllocated && m_pAlloc, "{}, {}", m_bDataAllocated, m_pAlloc);

    String r;
    r.m_pData = m_pData;
    r.m_size = m_size;
    return r;
}

inline void
Builder::reset() noexcept
{
    m_size = 0;
}

inline void
Builder::destroy() noexcept
{
    if (m_pAlloc && m_bDataAllocated)
        m_pAlloc->free(m_pData);
}

inline isize
Builder::push(char c)
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
Builder::push(const Span<const char> sp)
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
Builder::push(const StringView sv)
{
    return push(Span{sv.data(), sv.size()});
}

inline isize
Builder::pushN(const char c, const isize nTimes)
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
Builder::grow(isize newCap)
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
shorterSourcePath(const char* ntsSourcePath)
{
    static const StringView svCwd = currentWorkingDirectory();
    return ntsSourcePath + svCwd.size() + 1;
}

inline isize
printArgs(Context* pCtx)
{
    const StringView svFmtSlice = pCtx->fmt.subString(pCtx->fmtIdx, pCtx->fmt.size() - pCtx->fmtIdx);
    if (pCtx->pBuffer->push(svFmtSlice) != -1)
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
copyBackToContext(Context* pCtx, FormatArgs fmtArgs, const StringView sv)
{
    isize i = 0;
    const char filler = fmtArgs.filler ? fmtArgs.filler : ' ';

    auto clCopySpan = [&]
    {
        const isize mLen = utils::min(sv.size(), isize(fmtArgs.maxLen));
        if (pCtx->pBuffer->push(Span{sv.data(), mLen}) != -1)
            i += mLen;
    };

    if (bool(fmtArgs.eFmtFlags & FormatArgs::FLAGS::JUSTIFY_RIGHT))
    {
        /* leave space for the string */
        const isize nSpaces = fmtArgs.maxLen - sv.size();
        isize j = 0;

        if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen > i && nSpaces > 0)
        {
            if (pCtx->pBuffer->pushN(filler, nSpaces) != -1)
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
            if (pCtx->pBuffer->pushN(filler, fmtArgs.maxLen - i) != -1)
                i += fmtArgs.maxLen;
        }
    }

    return i;
}

inline isize
format(Context* pCtx, FormatArgs fmtArgs, const StringView sv)
{
    return copyBackToContext(pCtx, fmtArgs, sv);
}

template<typename STRING_T>
requires ConvertsToStringView<STRING_T>
inline isize
format(Context* pCtx, FormatArgs fmtArgs, const STRING_T& str)
{
    const isize realLen = strnlen(str.data(), str.size());
    return copyBackToContext(pCtx, fmtArgs, {const_cast<char*>(str.data()), realLen});
}

inline isize
format(Context* pCtx, FormatArgs fmtArgs, const char* str)
{
    return format(pCtx, fmtArgs, StringView(str));
}

inline isize
format(Context* pCtx, FormatArgs fmtArgs, char* const& pNullTerm)
{
    return format(pCtx, fmtArgs, StringView(pNullTerm));
}

inline isize
format(Context* pCtx, FormatArgs fmtArgs, bool b)
{
    return format(pCtx, fmtArgs, b ? "true" : "false");
}

inline isize
format(Context* pCtx, FormatArgs fmtArgs, const wchar_t x)
{
    char aBuff[8] {};
#ifdef _WIN32
    const isize n = snprintf(aBuff, utils::size(aBuff) - 1, "%lc", (wint_t)x);
#else
    const isize n = snprintf(aBuff, utils::size(aBuff) - 1, "%lc", x);
#endif

    return copyBackToContext(pCtx, fmtArgs, {aBuff, n});
}

inline isize
format(Context* pCtx, FormatArgs fmtArgs, const char32_t x)
{
    return format(pCtx, fmtArgs, (wchar_t)x);
}

inline isize
format(Context* pCtx, FormatArgs fmtArgs, const char x)
{
    char aBuff[4] {};
    const isize n = snprintf(aBuff, utils::size(aBuff), "%c", x);

    return copyBackToContext(pCtx, fmtArgs, {aBuff, n});
}

inline isize
format(Context* pCtx, FormatArgs fmtArgs, null)
{
    return format(pCtx, fmtArgs, StringView("nullptr"));
}

inline isize
format(Context* pCtx, FormatArgs fmtArgs, Empty)
{
    return format(pCtx, fmtArgs, StringView("Empty"));
}

template<typename T>
inline isize
format(Context* pCtx, FormatArgs fmtArgs, const T* const p)
{
    if (p == nullptr) return format(pCtx, fmtArgs, nullptr);

    fmtArgs.eFmtFlags |= FormatArgs::FLAGS::HASH;
    fmtArgs.eBase = BASE::SIXTEEN;
    return format(pCtx, fmtArgs, usize(p));
}

namespace details
{

template<typename T>
inline constexpr void
printArg(isize& rNWritten, isize& rI, bool& rbArg, Context* pCtx, const T& rArg)
{
    for (; rI < pCtx->fmt.size(); ++rI, ++rNWritten)
    {
        FormatArgs fmtArgs {};

        if (bool(pCtx->eFlags & Context::FLAGS::UPDATE_FMT_ARGS))
        {
            pCtx->eFlags &= ~Context::FLAGS::UPDATE_FMT_ARGS;

            fmtArgs = pCtx->prevFmtArgs;
            isize addBuff = format(pCtx, fmtArgs, rArg);

            rNWritten += addBuff;

            break;
        }

        const StringView svFmtSlice = pCtx->fmt.subString(rI, pCtx->fmt.size() - rI);
        const isize openBraceI = svFmtSlice.charAt('{');
        const StringView svFmtUntilOpenBrace = svFmtSlice.subString(0, openBraceI == -1 ? svFmtSlice.size() : openBraceI);

        pCtx->pBuffer->push(svFmtUntilOpenBrace);
        rI += svFmtUntilOpenBrace.size();
        rNWritten += svFmtUntilOpenBrace.size();

        /* No '{' case. */
        if (rI >= pCtx->fmt.size()) break;

        rbArg = true;
        if (rI + 1 < pCtx->fmt.size() && pCtx->fmt[rI + 1] == '{')
        {
            rI += 1, rNWritten += 1;
            rbArg = false;
        }

        if (rbArg)
        {
            isize addBuff = 0;
            const isize add = parseFormatArg(&fmtArgs, pCtx->fmt, rI);

            if (bool(fmtArgs.eFmtFlags & FormatArgs::FLAGS::ARG_IS_FMT))
            {
                if constexpr (std::is_integral_v<std::remove_reference_t<decltype(rArg)>>)
                {
                    if (bool(fmtArgs.eFmtFlags & FormatArgs::FLAGS::FLOAT_PRECISION_ARG))
                        fmtArgs.maxFloatLen = rArg;
                    else fmtArgs.maxLen = rArg;

                    pCtx->prevFmtArgs = fmtArgs;
                    pCtx->eFlags |= Context::FLAGS::UPDATE_FMT_ARGS;
                }
            }
            else
            {
                addBuff = format(pCtx, fmtArgs, rArg);
            }

            rI += add;
            rNWritten += addBuff;

            break;
        }
        else
        {
            pCtx->pBuffer->push(pCtx->fmt[rI]);
        }
    }
}

inline isize
formatVariadic(Context*, FormatArgs) noexcept
{
    return 0;
}

template<typename T>
inline isize
formatVariadic(Context* pCtx, FormatArgs fmtArgs, const T& x)
{
    return format(pCtx, fmtArgs, x);
}

template<typename T, typename ...ARGS>
inline isize
formatVariadic(Context* pCtx, FormatArgs fmtArgs, const T& first, const ARGS&... args)
{
    isize n = format(pCtx, fmtArgs, first);
    if (n < 0) return n;

    if (pCtx->pBuffer->push(StringView{", "}) == -1) return n;
    n += 2;

    return n + details::formatVariadic(pCtx, fmtArgs, args...);
}

inline isize
formatVariadicStacked(Context*, FormatArgs) noexcept
{
    return 0;
}

template<typename T>
inline isize
formatVariadicStacked(Context* pCtx, FormatArgs fmtArgs, const T& x)
{
    return format(pCtx, fmtArgs, x);
}

template<typename T, typename ...ARGS>
inline isize
formatVariadicStacked(Context* pCtx, FormatArgs fmtArgs, const T& first, const ARGS&... args)
{
    isize n = format(pCtx, fmtArgs, first);
    if (n < 0) return n;

    return n + details::formatVariadicStacked(pCtx, fmtArgs, args...);
}

template<typename T>
inline isize
formatFloat(Context* pCtx, FormatArgs fmtArgs, const T x)
{
    char aBuff[64] {};
    std::to_chars_result res {};
    if (fmtArgs.maxFloatLen == NPOS8)
        res = std::to_chars(aBuff, aBuff + sizeof(aBuff), x);
    else res = std::to_chars(aBuff, aBuff + sizeof(aBuff), x, std::chars_format::fixed, fmtArgs.maxFloatLen);

    if (res.ptr) return copyBackToContext(pCtx, fmtArgs, {aBuff, res.ptr - aBuff});
    else return copyBackToContext(pCtx, fmtArgs, {aBuff});
}

template<bool B_OPEN>
inline isize
pushOpenCloseFlag(Context* pCtx, const FormatArgs::FLAGS e)
{
    using F = FormatArgs::FLAGS;
    char aBuff[8] {};
    isize n = 0;

    if constexpr (B_OPEN)
    {
        if (bool(e & F::SQUARE_BRACKETS))
            n = print::toBuffer(aBuff + n, sizeof(aBuff) - n, "[");
        else if (bool(e & F::PARENTHESES))
            n = print::toBuffer(aBuff + n, sizeof(aBuff) - n, "(");
    }
    else
    {
        if (bool(e & F::SQUARE_BRACKETS))
            n = print::toBuffer(aBuff + n, sizeof(aBuff) - n, "]");
        else if (bool(e & F::PARENTHESES))
            n = print::toBuffer(aBuff + n, sizeof(aBuff) - n, ")");
    }

    if (pCtx->pBuffer->push(StringView{aBuff, n}) != -1)
        return n;

    return 0;
};

} /* namespace details */

template<typename T> requires (std::is_integral_v<T>)
inline constexpr isize
format(Context* pCtx, FormatArgs fmtArgs, const T x)
{
    char aBuff[64] {};
    const isize n = intToBuffer(x, {aBuff}, fmtArgs);
    if (fmtArgs.maxLen != NPOS16 && fmtArgs.maxLen < utils::size(aBuff) - 1)
        aBuff[fmtArgs.maxLen] = '\0';

    return copyBackToContext(pCtx, fmtArgs, {aBuff, n});
}

inline isize
format(Context* pCtx, FormatArgs fmtArgs, const f32 x)
{
    return details::formatFloat<f32>(pCtx, fmtArgs, x);
}

inline isize
format(Context* pCtx, FormatArgs fmtArgs, const f64 x)
{
    return details::formatFloat<f64>(pCtx, fmtArgs, x);
}

template<typename T, typename ...ARGS_T>
inline constexpr isize
printArgs(Context* pCtx, const T& tFirst, const ARGS_T&... tArgs)
{
    isize nWritten = 0;
    bool bArg = false;
    isize i = pCtx->fmtIdx;

    if (pCtx->fmtIdx >= pCtx->fmt.size())
    {
        /* NOTE: Edge case, when we need to fill but fmt is out of range. */
        if (bool(pCtx->eFlags & Context::FLAGS::UPDATE_FMT_ARGS))
            return format(pCtx, pCtx->prevFmtArgs, tFirst);
        else return 0;
    }

    details::printArg(nWritten, i, bArg, pCtx, tFirst);

    pCtx->fmtIdx = i;
    nWritten += printArgs(pCtx, tArgs...);

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
    Builder buff {pAlloc, aPreallocated, sizeof(aPreallocated) - 1};

    try
    {
        Context pCtx {.fmt = fmt, .pBuffer = &buff};
        const isize r = printArgs(&pCtx, tArgs...);
        fwrite(pCtx.pBuffer->m_pData, r, 1, fp);
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

    Builder buff {pBuff, buffSize};

    Context pCtx {.fmt = fmt, .pBuffer = &buff};
    printArgs(&pCtx, tArgs...);

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
toString(IAllocator* pAlloc, const StringView fmt, const ARGS_T&... tArgs)
{
    return toString(pAlloc, 0, fmt, tArgs...);
}

template<typename ...ARGS_T>
[[nodiscard]] inline String
toString(IAllocator* pAlloc, isize prealloc, const StringView fmt, const ARGS_T&... tArgs)
{
    Builder buff;

    try
    {
        new(&buff) Builder {pAlloc, prealloc};
        Context pCtx {.fmt = fmt, .pBuffer = &buff};
        printArgs(&pCtx, tArgs...);
        buff.push('\0');
        buff.m_size -= 1;
    }
    catch (const AllocException& ex)
    {
#ifdef ADT_DBG_MEMORY
        ex.printErrorMsg(stderr);
#endif
        if (buff.m_size > 0) buff.m_pData[--buff.m_size] = '\0';
        else return {};
    }

    return String(buff);
}

template<typename ...ARGS_T>
inline StringView
toBuilder(Builder* pBuffer, const StringView fmt, const ARGS_T&... tArgs)
{
    ADT_ASSERT(pBuffer != nullptr, "");

    try
    {
        Context pCtx {.fmt = fmt, .pBuffer = pBuffer};
        printArgs(&pCtx, tArgs...);
        pBuffer->push('\0');
        pBuffer->m_size -= 1;
    }
    catch (const AllocException& ex)
    {
#ifdef ADT_DBG_MEMORY
        ex.printErrorMsg(stderr);
#endif
        if (pBuffer->m_size > 0) pBuffer->m_pData[--pBuffer->m_size] = '\0';
        else return {};
    }

    return StringView(*pBuffer);
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
formatExpSize(Context* pCtx, FormatArgs fmtArgs, const auto& x, const isize contSize)
{
    if (pCtx->pBuffer->push('[') < 0) return 0;

    isize nWritten = 1;
    isize i = 0;

    for (const auto& e : x)
    {
        const isize n = format(pCtx, fmtArgs, e);
        if (n < 0) break;

        nWritten += n;

        if (i < contSize - 1)
        {
            const StringView svMore = ", ";
            if (pCtx->pBuffer->push(svMore) != -1) nWritten += svMore.size();
        }

        ++i;
    }

    if (pCtx->pBuffer->push(']') >= 0) ++nWritten;

    return nWritten;
}

inline isize
formatUntilEnd(Context* pCtx, FormatArgs fmtArgs, const auto& x)
{
    if (!x.data()) return copyBackToContext(pCtx, fmtArgs, "[]");

    if (pCtx->pBuffer->push('[') < 0) return 0;
    isize nWritten = 1;

    for (auto it = x.begin(); it != x.end(); ++it)
    {
        const isize n = format(pCtx, fmtArgs, *it);
        if (n < 0) break;

        nWritten += n;

        if (it.next() != x.end())
        {
            const StringView svMore = ", ";
            if (pCtx->pBuffer->push(svMore) != -1) nWritten += svMore.size();
        }
    }

    if (pCtx->pBuffer->push(']') >= 0) ++nWritten;

    return nWritten;
}

template<typename ...ARGS>
inline isize
formatVariadic(Context* pCtx, FormatArgs fmtArgs, const ARGS&... args)
{
    isize n = 0;

    n += details::pushOpenCloseFlag<true>(pCtx, fmtArgs.eFmtFlags);

    const isize nFormatted = details::formatVariadic(pCtx, fmtArgs, args...);
    if (nFormatted < 0) return n;

    n += nFormatted;
    n += details::pushOpenCloseFlag<false>(pCtx, fmtArgs.eFmtFlags);

    return n;
}

template<typename ...ARGS>
inline isize
formatVariadicStacked(Context* pCtx, FormatArgs fmtArgs, const ARGS&... args)
{
    isize n = 0;

    const isize nFormatted = details::formatVariadicStacked(pCtx, fmtArgs, args...);
    if (nFormatted < 0) return n;

    n += nFormatted;

    return n;
}

template<typename T>
requires (HasSizeMethod<T> && !ConvertsToStringView<T>)
inline isize
format(Context* pCtx, FormatArgs fmtArgs, const T& x)
{
    return formatExpSize(pCtx, fmtArgs, x, x.size());
}

template<typename T>
requires HasNextIt<T>
inline isize
format(Context* pCtx, FormatArgs fmtArgs, const T& x)
{
    return print::formatUntilEnd(pCtx, fmtArgs, x);
}

template<typename T, isize N>
inline isize
format(Context* pCtx, FormatArgs fmtArgs, const T (&a)[N])
{
    return formatExpSize(pCtx, fmtArgs, a, N);
}

template<typename T>
requires (!Printable<T>)
inline isize
format(Context* pCtx, FormatArgs fmtArgs, const T&)
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

    return format(pCtx, fmtArgs, svDemangled);
}

} /* namespace adt::print */

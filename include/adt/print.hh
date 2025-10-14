#pragma once

#include "print-inl.hh"

#include "IAllocator.hh"
#include "defer.hh"
#include "Array.hh"
#include "String.hh" /* IWYU pragma: keep */

#include <cctype>
#include <charconv>

namespace adt::print
{

namespace details
{

template<typename T>
inline void eatFmtArg(T r, FmtArgs* pFmtArgs) noexcept;

template<typename T>
inline TypeErasedArg createTypeErasedArg(const T& arg);

inline isize parseNumber(Context* pCtx, FmtArgs* pFmtArgs);

inline isize parseChar(Context* pCtx, FmtArgs* pFmtArgs);

inline isize parseColon(Context* pCtx, FmtArgs* pFmtArgs);

inline isize parseArg(Context* pCtx, FmtArgs* pFmtArgs);

inline isize parseArgs(Context* pCtx, const FmtArgs& fmtArgs);

} /* namespace details */

inline
Builder::Builder(IAllocator* pAlloc)
    : m_pAlloc{pAlloc},
      m_pData{},
      m_size{},
      m_cap{},
      m_bAllocated{}
{
}

inline
Builder::Builder(IAllocator* pAlloc, isize prealloc)
    : m_pAlloc{pAlloc},
      m_size{}
{
    const isize minSize = utils::min(8ll, prealloc + 1);
    m_pData = pAlloc->mallocV<char>(minSize);
    m_cap = minSize;
    m_bAllocated = true;
}

inline
Builder::Builder(Span<char> spBuff)
    : m_pAlloc{},
      m_pData{spBuff.data()},
      m_size{},
      m_cap{spBuff.size()},
      m_bAllocated{}
{
}

inline
Builder::Builder(IAllocator* pAlloc, Span<char> spBuff)
    : m_pAlloc{pAlloc},
      m_pData{spBuff.m_pData},
      m_size{},
      m_cap{spBuff.m_size},
      m_bAllocated{}
{
}

inline void
Builder::destroy() noexcept
{
    if (m_bAllocated) m_pAlloc->free(m_pData, m_cap);
    *this = {};
}

inline isize
Builder::push(char c)
{
    if (m_size + 1 >= m_cap) if (!growIfNeeded((m_cap + 1) * 2)) return -1;

    m_pData[m_size++] = c;
    m_pData[m_size] = '\0';
    return 1;
}

inline isize
Builder::push(const StringView sv)
{
    if (sv.m_size + m_size >= m_cap)
    {
        if (!m_pAlloc)
        {
            const isize maxPossbile = m_cap - 1 - m_size;
            if (maxPossbile <= 0) return -1;
            ::memcpy(m_pData + m_size, sv.m_pData, maxPossbile);
            m_size += maxPossbile;
            m_pData[m_cap - 1] = '\0';
            return maxPossbile;
        }

        if (!growIfNeeded((m_size + sv.m_size + 1) * 2)) return -1;
    }

    ::memcpy(m_pData + m_size, sv.m_pData, sv.m_size);
    m_size += sv.m_size;
    m_pData[m_size] = '\0';
    return sv.m_size;
}

inline isize
Builder::push(FmtArgs* pFmtArgs, StringView sv)
{
    const isize maxSvLen = pFmtArgs->maxLen > 0 ? utils::min(pFmtArgs->maxLen, sv.m_size) : sv.m_size;
    isize padSize = utils::max(pFmtArgs->padding, pFmtArgs->maxLen);
    if (padSize > 0) padSize -= maxSvLen;
    else padSize = 0;

    isize nWritten = 0;

    sv = sv.subString(0, maxSvLen);

    if (sv.m_size + m_size + padSize >= m_cap)
    {
        if (!m_pAlloc)
        {
            isize maxPossbile = m_cap - 1 - m_size;
            if (maxPossbile <= 0) goto done;

            if (bool(pFmtArgs->eFlags & FmtArgs::FLAGS::JUSTIFY_RIGHT))
            {
                if (padSize > 0)
                {
                    isize maxPad = utils::min(maxPossbile, padSize);
                    ::memset(m_pData + m_size, pFmtArgs->filler, maxPad);
                    m_size += maxPad;
                    nWritten += maxPad;
                    maxPossbile -= maxPad;
                }
                if (maxPossbile > 0)
                {
                    isize maxSv = utils::min(maxPossbile, sv.m_size);
                    ::memcpy(m_pData + m_size, sv.m_pData, maxSv);
                    m_size += maxSv;
                    nWritten += maxSv;
                }
            }
            else
            {
                isize maxSv = utils::min(maxPossbile, sv.m_size);
                ::memcpy(m_pData + m_size, sv.m_pData, maxSv);
                m_size += maxSv;
                nWritten += maxSv;
                maxPossbile -= maxSv;
                if (maxPossbile > 0)
                {
                    isize maxPad = utils::min(maxPossbile, padSize);
                    ::memset(m_pData + m_size, pFmtArgs->filler, maxPad);
                    m_size += maxPad;
                    nWritten += maxPad;
                }
            }

            goto done;
        }

        if (!growIfNeeded((sv.m_size + m_size + padSize + 1) * 2)) return -1;
    }

    if (bool(pFmtArgs->eFlags & FmtArgs::FLAGS::JUSTIFY_RIGHT))
    {
        if (padSize > 0)
        {
            ::memset(m_pData + m_size, pFmtArgs->filler, padSize);
            m_size += padSize;
            nWritten += padSize;
        }
        ::memcpy(m_pData + m_size, sv.m_pData, sv.m_size);
        m_size += sv.m_size;
        nWritten += sv.m_size;
    }
    else
    {
        ::memcpy(m_pData + m_size, sv.m_pData, sv.m_size);
        m_size += sv.m_size;
        nWritten += sv.m_size;
        if (padSize > 0)
        {
            ::memset(m_pData + m_size, pFmtArgs->filler, padSize);
            m_size += padSize;
            nWritten += padSize;
        }
    }

done:
    m_pData[m_size] = '\0';
    return nWritten;
}

template<typename ...ARGS>
inline isize
Builder::pushFmt(const StringView svFmt, const ARGS&... args)
{
    FmtArgs fmtArgs {};
    return pushFmt(&fmtArgs, svFmt, args...);
}

template<typename ...ARGS>
inline isize
Builder::pushFmt(FmtArgs* pFmtArgs, const StringView svFmt, const ARGS&... args)
{
    Array<TypeErasedArg, sizeof...(ARGS)> aArgs {details::createTypeErasedArg(args)...};
    Context ctx {.spArgs = aArgs, .svFmt = svFmt, .pBuilder = this};
    return details::parseArgs(&ctx, *pFmtArgs);
}

template<typename ...ARGS>
inline StringView
Builder::print(const StringView svFmt, const ARGS&... args)
{
    FmtArgs fmtArgs {};
    return print(&fmtArgs, svFmt, args...);
}

template<typename ...ARGS>
inline StringView
Builder::print(FmtArgs* pFmtArgs, const StringView svFmt, const ARGS&... args)
{
    Array<TypeErasedArg, sizeof...(ARGS)> aArgs {details::createTypeErasedArg(args)...};
    Context ctx {.spArgs = aArgs, .svFmt = svFmt, .pBuilder = this};
    const isize startI = m_size;
    const isize n = details::parseArgs(&ctx, *pFmtArgs);
    return StringView{m_pData + startI, n};
}

inline bool
Builder::growIfNeeded(isize newCap)
{
    if (!m_pAlloc) return false;

    try
    {
        if (!m_bAllocated)
        {
            char* pNewData = m_pAlloc->mallocV<char>(newCap);
            ::memcpy(pNewData, m_pData, m_size);
            m_pData = pNewData;
            m_cap = newCap;
        }
        else
        {
            m_pData = m_pAlloc->reallocV<char>(m_pData, m_size, newCap);
            m_cap = newCap;
        }
    }
    catch (const std::bad_alloc& ex)
    {
        return false;
    }

    return true;
}

template<typename T>
constexpr const StringView typeName()
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

template<typename T>
inline isize
format(Context* pCtx, FmtArgs*, const T&)
{
    const StringView sv = typeName<T>();

#if defined __clang__ || __GNUC__

    const StringView svSub = "T = ";
    const isize atI = sv.subStringAt(svSub);
    const StringView svDemangled = [&] {
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
    const StringView svDemangled = [&] {
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

    return pCtx->pBuilder->push(svDemangled);
}

namespace details
{

template<typename T>
inline void
eatFmtArg(T r, FmtArgs* pFmtArgs) noexcept
{
    if (bool(pFmtArgs->eFlags & FmtArgs::FLAGS::FLOAT_PRECISION))
    {
        pFmtArgs->eFlags &= ~FmtArgs::FLAGS::FLOAT_PRECISION;
        pFmtArgs->floatPrecision = r;
    }
    else if (bool(pFmtArgs->eFlags & FmtArgs::FLAGS::FILLER))
    {
        pFmtArgs->eFlags &= ~FmtArgs::FLAGS::FILLER;
        pFmtArgs->filler = r;
    }
    else if (bool(pFmtArgs->eFlags & FmtArgs::FLAGS::JUSTIFY_LEFT))
    {
        pFmtArgs->padding = r;
    }
    else if (bool(pFmtArgs->eFlags & FmtArgs::FLAGS::JUSTIFY_RIGHT))
    {
        pFmtArgs->padding = r;
    }
    else
    {
        pFmtArgs->maxLen = r;
    }
}

template<typename T>
inline TypeErasedArg
createTypeErasedArg(const T& arg)
{
    TypeErasedArg::PfnFormat pfn = [](Context* pCtx, FmtArgs* pFmtArgs, const void* pArg) {
        const T& r = *static_cast<const T*>(pArg);

        if (bool(pFmtArgs->eFlags & FmtArgs::FLAGS::COLON))
        {
            if constexpr (std::is_integral_v<T>)
            {
                if constexpr (std::is_unsigned_v<T>) eatFmtArg((usize)r, pFmtArgs);
                else eatFmtArg((isize)r, pFmtArgs);
                return FMT_ARG_SET;
            }
        }

        return format(pCtx, pFmtArgs, r);;
    };

    return TypeErasedArg{
        .pfnFormat = pfn,
        .pArg = &arg,
    };
}

inline isize
parseNumber(Context* pCtx, FmtArgs* pFmtArgs)
{
    isize nWritten = 0;
    isize startI = pCtx->fmtI;

    if (pCtx->fmtI < pCtx->svFmt.m_size && pCtx->svFmt[pCtx->fmtI] == '{')
    {
        ++pCtx->fmtI;
        nWritten += parseArg(pCtx, pFmtArgs);
    }
    else
    {
        while (pCtx->fmtI < pCtx->svFmt.m_size && std::isdigit(pCtx->svFmt[pCtx->fmtI]))
            ++pCtx->fmtI;

        const StringView svNum = pCtx->svFmt.subString(startI, pCtx->fmtI - startI);

        if (bool(pFmtArgs->eFlags & FmtArgs::FLAGS::JUSTIFY_LEFT))
        {
            pFmtArgs->padding = svNum.toI64();
        }
        else if (bool(pFmtArgs->eFlags & FmtArgs::FLAGS::JUSTIFY_RIGHT))
        {
            pFmtArgs->padding = svNum.toI64();
        }
        else if (bool(pFmtArgs->eFlags & FmtArgs::FLAGS::FLOAT_PRECISION))
        {
            pFmtArgs->eFlags &= ~FmtArgs::FLAGS::FLOAT_PRECISION;
            pFmtArgs->floatPrecision = svNum.toI64();
        }
        else
        {
            pFmtArgs->maxLen = svNum.toI64();
        }
    }

    return nWritten;
}

inline isize
parseChar(Context* pCtx, FmtArgs* pFmtArgs)
{
    isize nWritten = 0;

    if (pCtx->fmtI >= pCtx->svFmt.m_size) return 0;

    if (pCtx->svFmt[pCtx->fmtI] == '{')
    {
        pFmtArgs->eFlags |= FmtArgs::FLAGS::FILLER;
        ++pCtx->fmtI;
        nWritten += parseArg(pCtx, pFmtArgs);
    }
    else
    {
        pFmtArgs->filler = pCtx->svFmt[pCtx->fmtI];
    }

    return nWritten;
}

inline isize
parseColon(Context* pCtx, FmtArgs* pFmtArgs)
{
    while (pCtx->fmtI < pCtx->svFmt.m_size && pCtx->svFmt[pCtx->fmtI] != '}')
    {
        if (pCtx->svFmt[pCtx->fmtI] == '{')
        {
            ++pCtx->fmtI;
            parseArg(pCtx, pFmtArgs);
            continue;
        }
        else if (std::isdigit(pCtx->svFmt[pCtx->fmtI]))
        {
            parseNumber(pCtx, pFmtArgs);
            continue;
        }
        else if (pCtx->svFmt[pCtx->fmtI] == 'f')
        {
            ++pCtx->fmtI;
            parseChar(pCtx, pFmtArgs);
            continue;
        }
        else if (pCtx->svFmt[pCtx->fmtI] == '<')
        {
            ++pCtx->fmtI;
            pFmtArgs->eFlags |= FmtArgs::FLAGS::JUSTIFY_LEFT;
            parseNumber(pCtx, pFmtArgs);
            continue;
        }
        else if (pCtx->svFmt[pCtx->fmtI] == '>')
        {
            ++pCtx->fmtI;
            pFmtArgs->eFlags |= FmtArgs::FLAGS::JUSTIFY_RIGHT;
            parseNumber(pCtx, pFmtArgs);
            continue;
        }
        else if (pCtx->svFmt[pCtx->fmtI] == '.')
        {
            ++pCtx->fmtI;
            pFmtArgs->eFlags |= FmtArgs::FLAGS::FLOAT_PRECISION;
            parseNumber(pCtx, pFmtArgs);
            continue;
        }
        else if (pCtx->svFmt[pCtx->fmtI] == '#')
        {
            pFmtArgs->eFlags |= FmtArgs::FLAGS::HASH;
        }
        else if (pCtx->svFmt[pCtx->fmtI] == 'b')
        {
            pFmtArgs->eBase = FmtArgs::BASE::TWO;
        }
        else if (pCtx->svFmt[pCtx->fmtI] == 'o')
        {
            pFmtArgs->eBase = FmtArgs::BASE::EIGHT;
        }
        else if (pCtx->svFmt[pCtx->fmtI] == 'x')
        {
            pFmtArgs->eBase = FmtArgs::BASE::SIXTEEN;
        }

        ++pCtx->fmtI;
    }

    return 0;
}

inline isize
parseArg(Context* pCtx, FmtArgs* pFmtArgs)
{
    isize nWritten = 0;

    while (pCtx->fmtI < pCtx->svFmt.m_size && pCtx->svFmt[pCtx->fmtI] != '}')
    {
        if (pCtx->svFmt[pCtx->fmtI] == ':')
        {
            pFmtArgs->eFlags |= FmtArgs::FLAGS::COLON;
            ++pCtx->fmtI;
            isize n = parseColon(pCtx, pFmtArgs);
            if (n > 0) nWritten += n;
            pFmtArgs->eFlags &= ~FmtArgs::FLAGS::COLON;
            continue;
        }

        ++pCtx->fmtI;
    }

    if (pCtx->argI >= 0 && pCtx->argI < pCtx->spArgs.m_size)
    {
        auto arg = pCtx->spArgs[pCtx->argI++];
        isize n = arg.pfnFormat(pCtx, pFmtArgs, arg.pArg);
        if (n != FMT_ARG_SET && n > 0) nWritten += n;
    }
    else
    {
        isize n = pCtx->pBuilder->push("(missing arg)");
        if (n > 0) nWritten += n;
    }

    if (pCtx->fmtI < pCtx->svFmt.m_size) ++pCtx->fmtI;

    return nWritten;
}

inline isize
parseArgs(Context* pCtx, const FmtArgs& fmtArgs)
{
    isize nWritten = 0;

    while (pCtx->fmtI < pCtx->svFmt.m_size)
    {
        StringView svCurr = pCtx->svFmt.subString(pCtx->fmtI);
        const isize nextParenOff = svCurr.charAt('{');
        if (nextParenOff == -1) break;

        {
            const isize n = pCtx->pBuilder->push(svCurr.subString(0, nextParenOff));
            if (n <= -1) return nWritten;
            nWritten += n;
        }

        pCtx->fmtI += nextParenOff;
        ++pCtx->fmtI;
        if (pCtx->fmtI < pCtx->svFmt.m_size && pCtx->svFmt[pCtx->fmtI] == '{') /* Skip arg on double {{. */
        {
            pCtx->pBuilder->push(pCtx->svFmt[pCtx->fmtI]);
            ++pCtx->fmtI;
        }
        else
        {
            FmtArgs fmtArgs2 = fmtArgs;
            nWritten += parseArg(pCtx, &fmtArgs2);
        }
    }

    if (pCtx->fmtI < pCtx->svFmt.m_size)
    {
        StringView svCurr = pCtx->svFmt.subString(pCtx->fmtI);
        isize n = pCtx->pBuilder->push(svCurr);
        if (n > 0) nWritten += n;
    }

    return nWritten;
}

} /* namespace details */;

template<std::integral T>
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const T& arg)
{
    char aBuff[256];
    isize nWritten = 0;

    static const char s_aCharSet[] = "0123456789abcdef";

#define _ITOA_(x, base)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        if (nWritten >= (isize)sizeof(aBuff))                                                                          \
            goto done;                                                                                                 \
        aBuff[nWritten++] = s_aCharSet[x % base];                                                                      \
        x /= base;                                                                                                     \
    } while (x > 0)

    T num = arg;
    if constexpr (!std::is_unsigned_v<T>)
        if (arg < 0) num = -num;

    if (pFmtArgs->eBase == FmtArgs::BASE::TEN) { _ITOA_(num, 10); }
    else if (pFmtArgs->eBase == FmtArgs::BASE::TWO) { _ITOA_(num, 2); }
    else if (pFmtArgs->eBase == FmtArgs::BASE::EIGHT) { _ITOA_(num, 8); }
    else if (pFmtArgs->eBase == FmtArgs::BASE::SIXTEEN) { _ITOA_(num, 16); }
#undef _ITOA_

    if (bool(pFmtArgs->eFlags & FmtArgs::FLAGS::HASH) && pFmtArgs->eBase != FmtArgs::BASE::TEN)
    {
        if (pFmtArgs->eBase == FmtArgs::BASE::TWO)
        {
            if (nWritten + 2 <= (isize)sizeof(aBuff))
            {
                aBuff[nWritten++] = 'b';
                aBuff[nWritten++] = '0';
            }
        }
        else if (pFmtArgs->eBase == FmtArgs::BASE::EIGHT)
        {
            if (nWritten + 1 <= (isize)sizeof(aBuff))
                aBuff[nWritten++] = 'o';
        }
        else if (pFmtArgs->eBase == FmtArgs::BASE::SIXTEEN)
        {
            if (nWritten + 2 <= (isize)sizeof(aBuff))
            {
                aBuff[nWritten++] = 'x';
                aBuff[nWritten++] = '0';
            }
        }
    }

    if constexpr (!std::is_unsigned_v<T>)
        if (arg < 0 && nWritten < (isize)sizeof(aBuff)) aBuff[nWritten++] = '-';

done:
    for (isize i = 0; i < nWritten >> 1; ++i)
        utils::swap(&aBuff[i], &aBuff[nWritten - 1 - i]);

    return pCtx->pBuilder->push(pFmtArgs, {aBuff, nWritten});
}

template<std::floating_point T>
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const T& arg)
{
    char aBuff[128] {};
    std::to_chars_result res {};

    if (pFmtArgs->floatPrecision == -1)
        res = std::to_chars(aBuff, aBuff + sizeof(aBuff), arg);
    else res = std::to_chars(aBuff, aBuff + sizeof(aBuff), arg, std::chars_format::fixed, pFmtArgs->floatPrecision);

    isize n = 0;
    if (res.ptr) n = pCtx->pBuilder->push(pFmtArgs, {aBuff, res.ptr - aBuff});
    else n = pCtx->pBuilder->push(pFmtArgs, {aBuff});

    return n;
}

template<>
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const char& c)
{
    static const char s_aCharSet[] = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
    const isize idx = (c >= 32 && c <= 126) ? c - 32 : 0;
    return pCtx->pBuilder->push(pFmtArgs, StringView{const_cast<char*>(s_aCharSet + idx), 1});
}

template<isize N>
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const char(&arg)[N])
{
    return format(pCtx, pFmtArgs, StringView{arg});
}

template<>
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const StringView& sv)
{
    return pCtx->pBuilder->push(pFmtArgs, sv);
}

template<>
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const char* const& sv)
{
    return format(pCtx, pFmtArgs, StringView{sv});
}

template<>
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const wchar_t& wc)
{
    char aBuff[16];
    mbstate_t mbState {};
    isize n = wcrtomb(aBuff, wc, &mbState);
    if (n < 0) n = 0;

    return format(pCtx, pFmtArgs, StringView{aBuff, n});
}

template<>
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const Span<const wchar_t>& spBuff)
{
    isize nWritten = 0;
    for (isize i = 0; i < spBuff.m_size && spBuff[i] != '\0'; ++i)
    {
        isize nn = format(pCtx, pFmtArgs, spBuff[i]);
        if (nn <= -1) break;
        nWritten += nn;
    }

    return nWritten;
}

template<isize N>
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const wchar_t(&aBuff)[N])
{
    return format(pCtx, pFmtArgs, Span<const wchar_t>{aBuff, N});
}

template<typename T>
requires (HasSizeMethod<T> && !ConvertsToStringView<T>)
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const T& x)
{
    isize nWritten = 0;
    const isize size = x.size();

    if (pCtx->pBuilder->push('[') <= -1) return nWritten;
    else ++nWritten;

    if (size > 0)
    {
        auto it = x.begin();
        const isize nn = format(pCtx, pFmtArgs, *it);
        if (nn <= -1) return nWritten;
        nWritten += nn;
        ++it;

        for (; it != x.end(); ++it)
        {
            isize nn = pCtx->pBuilder->push(", ");
            if (nn <= -1) return nWritten;
            nn = format(pCtx, pFmtArgs, *it);
            if (nn <= -1) return nWritten;
            nWritten += nn;
        }
    }

    if (pCtx->pBuilder->push(']') <= -1) return nWritten;
    else ++nWritten;

    return nWritten;
}

template<typename T>
requires (ConvertsToStringView<T>)
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const T& x)
{
    if constexpr (HasSizeMethod<T>)
        return format(pCtx, pFmtArgs, StringView{const_cast<char*>(x.data()), (isize)x.size()});
    else return format(pCtx, pFmtArgs, StringView{x});
}

template<typename T>
requires HasNextIt<T>
inline isize
format(Context* pCtx, FmtArgs* pFmtArgs, const T& x)
{
    isize nWritten = 0;

    if (pCtx->pBuilder->push('[') <= -1) return nWritten;
    else ++nWritten;

    auto end = x.end();
    for (auto it = x.begin(); it != end; ++it)
    {
        isize nn = format(pCtx, pFmtArgs, *it);
        if (nn <= -1) return nWritten;

        if (it.next() != end)
        {
            nn = pCtx->pBuilder->push(", ");
            if (nn <= -1) return nWritten;
        }

        nWritten += nn;
    }

    if (pCtx->pBuilder->push(']') <= -1) return nWritten;
    else ++nWritten;

    return nWritten;
}

template<typename ...ARGS>
inline isize
toSpan(Span<char> spBuff, const StringView svFmt, const ARGS&... args)
{
    Array<TypeErasedArg, sizeof...(ARGS)> aArgs {details::createTypeErasedArg(args)...};
    Builder builder {spBuff};
    Context ctx {.spArgs = aArgs, .svFmt = svFmt, .pBuilder = &builder};
    return details::parseArgs(&ctx, FmtArgs{});
}

template<typename ...ARGS>
inline isize
toBuffer(char* pBuff, isize buffSize, const StringView svFmt, const ARGS&... args)
{
    Array<TypeErasedArg, sizeof...(ARGS)> aArgs {details::createTypeErasedArg(args)...};
    Builder builder {Span{pBuff, buffSize}};
    Context ctx {.spArgs = aArgs, .svFmt = svFmt, .pBuilder = &builder};
    return details::parseArgs(&ctx, FmtArgs{});
}

template<isize PREALLOC, typename ...ARGS>
inline isize
toFILE(FILE* pFile, const StringView svFmt, const ARGS&... args)
{
    return toFILE(nullptr, pFile, svFmt, args...);
}

template<isize PREALLOC, typename ...ARGS>
inline isize
toFILE(IAllocator* pAlloc, FILE* pFile, const StringView svFmt, const ARGS&... args)
{
    Array<TypeErasedArg, sizeof...(ARGS)> aArgs {details::createTypeErasedArg(args)...};
    char aBuff[PREALLOC];
    Builder builder {pAlloc, Span{aBuff}};
    ADT_DEFER( builder.destroy() );

    Context ctx {.spArgs = aArgs, .svFmt = svFmt, .pBuilder = &builder};
    const isize n = details::parseArgs(&ctx, FmtArgs{});
    fwrite(builder.m_pData, builder.m_size, 1, pFile);

    return n;
}

template<typename ...ARGS>
inline isize
toFILE(IAllocator* pAlloc, isize prealloc, FILE* pFile, const StringView svFmt, const ARGS&... args)
{
    Array<TypeErasedArg, sizeof...(ARGS)> aArgs {details::createTypeErasedArg(args)...};
    Builder builder {pAlloc, prealloc};
    ADT_DEFER( builder.destroy() );

    Context ctx {.spArgs = aArgs, .svFmt = svFmt, .pBuilder = &builder};
    const isize n = details::parseArgs(&ctx, FmtArgs{});
    fwrite(builder.m_pData, builder.m_size, 1, pFile);

    return n;
}

template<typename ...ARGS>
inline isize
out(const StringView svFmt, const ARGS&... args)
{
    return toFILE(stdout, svFmt, args...);
}

template<typename ...ARGS>
inline isize
err(const StringView svFmt, const ARGS&... args)
{
    return toFILE(stderr, svFmt, args...);
}

} /* namespace adt::print2 */

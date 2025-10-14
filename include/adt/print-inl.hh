#pragma once

#include "String-inl.hh"
#include "enum.hh"

#include <cstdio>

namespace adt
{
struct IAllocator;
}

namespace adt::print
{

struct FmtArgs;

struct Builder
{
    IAllocator* m_pAlloc;
    char* m_pData;
    isize m_size;
    isize m_cap;
    bool m_bAllocated;

    /* */

    Builder() noexcept : m_pAlloc{}, m_pData{}, m_size{}, m_cap{}, m_bAllocated{} {}
    explicit Builder(IAllocator* pAlloc);
    Builder(IAllocator* pAlloc, isize prealloc);
    explicit Builder(Span<char> spBuff);
    Builder(IAllocator* pAlloc, Span<char> spBuff);

    /* */

    explicit operator StringView() const noexcept { return {m_pData, m_size}; }

    /* */

    void destroy() noexcept;

    isize size() const noexcept { return m_size; }
    isize cap() const noexcept { return m_cap; }
    void reset() noexcept { m_size = 0; }

    isize push(char c);
    isize push(const StringView sv);
    isize push(FmtArgs* pFmtArgs, StringView sv);

    template<typename ...ARGS>
    isize pushFmt(const StringView svFmt, const ARGS&... args);

    template<typename ...ARGS>
    isize pushFmt(FmtArgs* pFmtArgs, const StringView svFmt, const ARGS&... args);

    template<typename ...ARGS>
    StringView print(const StringView svFmt, const ARGS&... args);

    template<typename ...ARGS>
    StringView print(FmtArgs* pFmtArgs, const StringView svFmt, const ARGS&... args);

protected:
    bool growIfNeeded(isize newCap);
};

struct Context;

struct TypeErasedArg
{
    using PfnFormat = isize (*)(Context* pCtx, FmtArgs* pFmtArgs, const void* pArg);

    /* */

    PfnFormat pfnFormat {};
    const void* pArg {};
};

constexpr isize FMT_ARG_SET = -999;

struct FmtArgs
{
    enum class BASE : u8 { TWO = 2, EIGHT = 8, TEN = 10, SIXTEEN = 16 };
    enum class FLAGS : u8
    {
        COLON = 1,
        FLOAT_PRECISION = 1 << 1,
        JUSTIFY_LEFT = 1 << 2,
        JUSTIFY_RIGHT = 1 << 3,
        SHOW_SIGN = 1 << 4,
        FILLER = 1 << 5,
        HASH = 1 << 6, /* Display 0x for hex, o for octal, 0b for binary. */
    };

    /* */

    isize maxLen = -1;
    isize floatPrecision = -1;
    isize padding = 0;
    BASE eBase = BASE::TEN;
    FLAGS eFlags {};
    char filler = ' ';
    bool bJustifyRight = false;
};
ADT_ENUM_BITWISE_OPERATORS(FmtArgs::FLAGS);

struct Context
{
    Span<TypeErasedArg> spArgs {};
    isize argI {};
    const StringView svFmt {};
    isize fmtI {};
    Builder* pBuilder {};
};

template<typename T>
constexpr const StringView typeName();

inline const char* currentWorkingDirectory();

inline const char* shorterSourcePath(const char* ntsSourcePath);

template<typename T>
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const T& arg); /* Fallback template. */

template<std::integral T>
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const T& arg);

template<std::floating_point T>
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const T& arg);

template<>
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const char& c);

template<isize N>
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const char(&arg)[N]);

template<>
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const StringView& sv);

template<>
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const char* const& sv);

template<>
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const wchar_t& wc);

template<>
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const Span<const wchar_t>& spBuff);

template<isize N>
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const wchar_t(&aBuff)[N]);

template<typename T>
requires (HasSizeMethod<T> && !ConvertsToStringView<T>)
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const T& x);

template<typename T>
requires (ConvertsToStringView<T>)
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const T& x);

template<typename T>
requires HasNextIt<T>
inline isize format(Context* pCtx, FmtArgs* pFmtArgs, const T& x);

template<typename ...ARGS>
inline isize toSpan(Span<char> spBuff, const StringView svFmt, const ARGS&... args);

template<typename ...ARGS>
inline isize toBuffer(char* pBuff, isize buffSize, const StringView svFmt, const ARGS&... args);

template<isize PREALLOC = 128, typename ...ARGS>
inline isize toFILE(FILE* pFile, const StringView svFmt, const ARGS&... args);

template<isize PREALLOC = 128, typename ...ARGS>
inline isize toFILE(IAllocator* pAlloc, FILE* pFile, const StringView svFmt, const ARGS&... args);

template<typename ...ARGS>
inline isize toFILE(IAllocator* pAlloc, isize prealloc, FILE* pFile, const StringView svFmt, const ARGS&... args);

template<typename ...ARGS>
inline isize out(const StringView svFmt, const ARGS&... args);

template<typename ...ARGS>
inline isize err(const StringView svFmt, const ARGS&... args);

} /* namespace adt::print */

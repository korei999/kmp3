#pragma once

#include "String.inc"
#include "enum.hh"

#include <cstdio>
#include <type_traits>

namespace adt::print
{

enum class BASE : u8 { TWO = 2, EIGHT = 8, TEN = 10, SIXTEEN = 16 };

struct FormatArgs
{
    enum class FLAGS : u8
    {
        HASH = 1,
        ALWAYS_SHOW_SIGN = 1 << 1,
        ARG_IS_FMT = 1 << 2,
        FLOAT_PRECISION_ARG = 1 << 3,
        JUSTIFY_RIGHT = 1 << 4,
        SQUARE_BRACKETS = 1 << 5,
    };

    /* */

    u16 maxLen = NPOS16;
    u8 maxFloatLen = NPOS8;
    BASE eBase = BASE::TEN;
    FLAGS eFmtFlags {};
    char filler {};
};
ADT_ENUM_BITWISE_OPERATORS(FormatArgs::FLAGS);

struct Buffer
{
    IAllocator* m_pAlloc {};
    char* m_pData {};
    isize m_size {};
    isize m_cap {};
    bool m_bDataAllocated {}; /* Point to preallocated buffer at first, realloc later. */

    /* */

    Buffer() = default;
    Buffer(IAllocator* pAlloc, char* pBuff, isize buffSize) : m_pAlloc {pAlloc}, m_pData {pBuff}, m_cap {buffSize} {}
    Buffer(char* pBuff, isize buffSize) : m_pData {pBuff}, m_cap {buffSize} {}

    /* */

    isize push(char c);
};

struct Context
{
    enum class FLAGS : u8
    {
        NONE = 0,
        UPDATE_FMT_ARGS = 1,
    };

    /* */

    StringView fmt {};
    isize fmtIdx {};
    Buffer* pBuffer {};
    FormatArgs prevFmtArgs {};
    FLAGS eFlags {};
};
ADT_ENUM_BITWISE_OPERATORS(Context::FLAGS);

template<typename T>
constexpr const StringView typeName();

template<typename ...ARGS_T> inline isize out(const StringView fmt, const ARGS_T&... tArgs) noexcept;
template<typename ...ARGS_T> inline isize err(const StringView fmt, const ARGS_T&... tArgs) noexcept;

inline const char* stripSourcePath(const char* ntsSourcePath);

inline isize printArgs(Context ctx) noexcept;

inline isize parseFormatArg(FormatArgs* pArgs, const StringView fmt, isize fmtIdx) noexcept;

inline isize intToBuffer(isize x, Span<char> spBuff, FormatArgs fmtArgs) noexcept;

inline isize copyBackToContext(Context ctx, FormatArgs fmtArgs, const Span<char> spSrc) noexcept;

inline isize format(Context ctx, FormatArgs fmtArgs, const StringView str) noexcept;

template<typename STRING_T> requires ConvertsToStringView<STRING_T>
inline isize format(Context ctx, FormatArgs fmtArgs, const STRING_T& str) noexcept;

inline isize format(Context ctx, FormatArgs fmtArgs, const char* str) noexcept;

inline isize format(Context ctx, FormatArgs fmtArgs, char* const& pNullTerm) noexcept;

inline isize format(Context ctx, FormatArgs fmtArgs, bool b) noexcept;

template<typename INT_T> requires std::is_integral_v<INT_T>
inline constexpr isize format(Context ctx, FormatArgs fmtArgs, const INT_T& x) noexcept;

inline isize format(Context ctx, FormatArgs fmtArgs, const f32 x) noexcept;

inline isize format(Context ctx, FormatArgs fmtArgs, const f64 x) noexcept;

inline isize format(Context ctx, FormatArgs fmtArgs, const wchar_t x) noexcept;

inline isize format(Context ctx, FormatArgs fmtArgs, const char32_t x) noexcept;

inline isize format(Context ctx, FormatArgs fmtArgs, const char x) noexcept;

inline isize format(Context ctx, FormatArgs fmtArgs, null) noexcept;

inline isize format(Context ctx, FormatArgs fmtArgs, Empty) noexcept;

template<typename T>
inline isize format(Context ctx, FormatArgs fmtArgs, const T* const p) noexcept;

template<typename T, typename ...ARGS_T>
inline constexpr isize printArgs(Context ctx, const T& tFirst, const ARGS_T&... tArgs) noexcept;

template<isize SIZE = 512, typename ...ARGS_T>
inline isize toFILE(FILE* fp, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<isize SIZE = 512, typename ...ARGS_T>
inline isize toFILE(IAllocator* pAlloc, FILE* fp, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline constexpr isize toBuffer(char* pBuff, isize buffSize, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline constexpr isize toSpan(Span<char> sp, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline isize out(const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline isize err(const StringView fmt, const ARGS_T&... tArgs) noexcept;

inline isize FormatArgsToFmt(const FormatArgs fmtArgs, Span<char> spFmt) noexcept;

inline isize formatExpSize(Context ctx, FormatArgs fmtArgs, const auto& x, const isize contSize) noexcept;

inline isize formatUntilEnd(Context ctx, FormatArgs fmtArgs, const auto& x) noexcept;

template<typename ...ARGS>
inline isize formatVariadic(Context ctx, FormatArgs fmtArgs, const ARGS&... args) noexcept;

template<typename T>
requires (HasSizeMethod<T> && !ConvertsToStringView<T>)
inline isize format(Context ctx, FormatArgs fmtArgs, const T& x) noexcept;

template<typename T>
requires HasNextIt<T>
inline isize format(Context ctx, FormatArgs fmtArgs, const T& x) noexcept;

template<typename T, isize N>
inline isize format(Context ctx, FormatArgs fmtArgs, const T (&a)[N]) noexcept;

template<typename T>
concept Printable = requires(const T& c)
{ print::format({}, {}, c); };

template<typename T>
requires (!Printable<T>)
inline isize format(Context ctx, FormatArgs fmtArgs, const T&) noexcept;

} /* namespace adt::print */

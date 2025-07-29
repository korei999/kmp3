#pragma once

#include "String.inc"
#include "enum.hh"

#include <cstdio>
#include <type_traits>

namespace adt
{
template<typename A, typename B> struct Pair;
}

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
    SQUARE_BRACKETS = 1 << 5,
};
ADT_ENUM_BITWISE_OPERATORS(FMT_FLAGS);

enum class CONTEXT_FLAGS : u8
{
    NONE = 0,
    UPDATE_FMT_ARGS = 1,
};
ADT_ENUM_BITWISE_OPERATORS(CONTEXT_FLAGS);

struct FormatArgs;
struct Context;

template<typename T>
constexpr const StringView typeName();

template<typename ...ARGS_T> inline isize out(const StringView fmt, const ARGS_T&... tArgs) noexcept;
template<typename ...ARGS_T> inline isize err(const StringView fmt, const ARGS_T&... tArgs) noexcept;

inline const char* stripSourcePath(const char* ntsSourcePath);

inline isize printArgs(Context ctx) noexcept;

inline constexpr bool oneOfChars(const char x, const StringView chars) noexcept;

inline isize parseFormatArg(FormatArgs* pArgs, const StringView fmt, isize fmtIdx) noexcept;

template<typename INT_T> requires std::is_integral_v<INT_T>
inline constexpr isize intToBuffer(INT_T x, Span<char> spBuff, FormatArgs fmtArgs) noexcept;

inline isize copyBackToContext(Context ctx, FormatArgs fmtArgs, const Span<char> spSrc) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const StringView str) noexcept;

template<typename STRING_T> requires ConvertsToStringView<STRING_T>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const STRING_T& str) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const char* str) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, char* const& pNullTerm) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, bool b) noexcept;

template<typename INT_T> requires std::is_integral_v<INT_T>
inline constexpr isize formatToContext(Context ctx, FormatArgs fmtArgs, const INT_T& x) noexcept;

template<typename FLOAT_T> requires std::is_floating_point_v<FLOAT_T>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const FLOAT_T x) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const wchar_t x) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const char32_t x) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const char x) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, null) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, Empty) noexcept;

template<typename A, typename B>
inline u32 formatToContext(Context ctx, FormatArgs fmtArgs, const Pair<A, B>& x);

template<typename PTR_T> requires std::is_pointer_v<PTR_T>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const PTR_T p) noexcept;

template<typename T, typename ...ARGS_T>
inline constexpr isize printArgs(Context ctx, const T& tFirst, const ARGS_T&... tArgs) noexcept;

template<isize SIZE = 512, typename ...ARGS_T>
inline isize toFILE(FILE* fp, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline constexpr isize toBuffer(char* pBuff, isize buffSize, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline constexpr isize toString(StringView* pDest, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline constexpr isize toSpan(Span<char> sp, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline isize out(const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline isize err(const StringView fmt, const ARGS_T&... tArgs) noexcept;

inline isize FormatArgsToFmt(const FormatArgs fmtArgs, Span<char> spFmt) noexcept;

inline isize formatToContextExpSize(Context ctx, FormatArgs fmtArgs, const auto& x, const isize contSize) noexcept;

inline isize formatToContextUntilEnd(Context ctx, FormatArgs fmtArgs, const auto& x) noexcept;

template<typename ...ARGS>
inline isize formatToContextVariadic(Context ctx, FormatArgs fmtArgs, const ARGS&... args) noexcept;

template<typename T>
requires (HasSizeMethod<T> && !ConvertsToStringView<T>)
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const T& x) noexcept;

template<typename T>
requires HasNextIt<T>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const T& x) noexcept;

template<typename T, isize N>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const T (&a)[N]) noexcept;

template<typename T>
concept Printable = requires(const T& c)
{ print::formatToContext({}, {}, c); };

template<typename T>
requires (!Printable<T>)
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const T&) noexcept;

} /* namespace adt::print */

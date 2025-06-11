#pragma once

#include "String.inc"
#include "Pair.hh"

#include <cstdio>
#include <type_traits>

namespace adt::print
{

enum class BASE : u8;

enum class FMT_FLAGS : u8;

struct FormatArgs;
struct Context;

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

template<typename ...ARGS_T> inline isize out(const StringView fmt, const ARGS_T&... tArgs) noexcept;
template<typename ...ARGS_T> inline isize err(const StringView fmt, const ARGS_T&... tArgs) noexcept;

inline isize printArgs(Context ctx) noexcept;

inline constexpr bool oneOfChars(const char x, const StringView chars) noexcept;

inline isize parseFormatArg(FormatArgs* pArgs, const StringView fmt, isize fmtIdx) noexcept;

template<typename INT_T> requires std::is_integral_v<INT_T>
inline constexpr void intToBuffer(INT_T x, Span<char> spBuff, FormatArgs fmtArgs) noexcept;

inline isize copyBackToContext(Context ctx, FormatArgs fmtArgs, const Span<char> spSrc) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const StringView str) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const String str) noexcept;

template<int SIZE> requires(SIZE > 1)
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const StringFixed<SIZE>& str) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const char* str) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, char* const& pNullTerm) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, bool b) noexcept;

template<typename INT_T> requires std::is_integral_v<INT_T>
inline constexpr isize formatToContext(Context ctx, FormatArgs fmtArgs, const INT_T& x) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const f32 x) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const f64 x) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const wchar_t x) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const char32_t x) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const char x) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, null) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, Empty) noexcept;

template<typename A, typename B>
inline u32 formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const Pair<A, B>& x);

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

template<typename T>
requires HasSizeMethod<T>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const T& x) noexcept;

template<typename T>
requires HasNextIt<T>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const T& x) noexcept;

template<typename T, isize N>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const T (&a)[N]) noexcept;

template<typename T>
concept Printable = requires(const T& c)
{
    print::formatToContext({}, {}, c);
};

template<typename T>
requires (!Printable<T>)
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const T&) noexcept;

} /* namespace adt::print */

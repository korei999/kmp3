#pragma once

#include "StringDecl.hh"

#include <type_traits>

namespace adt::print
{

enum class BASE : u8;

enum class FMT_FLAGS : u8;

struct FormatArgs;
struct Context;

template<typename ...ARGS_T> inline isize out(const StringView fmt, const ARGS_T&... tArgs) noexcept;
template<typename ...ARGS_T> inline isize err(const StringView fmt, const ARGS_T&... tArgs) noexcept;

inline isize printArgs(Context ctx) noexcept;

inline constexpr bool oneOfChars(const char x, const StringView chars) noexcept;

inline isize parseFormatArg(FormatArgs* pArgs, const StringView fmt, isize fmtIdx) noexcept;

template<typename INT_T> requires std::is_integral_v<INT_T>
inline constexpr void intToBuffer(INT_T x, Span<char> spBuff, FormatArgs fmtArgs) noexcept;

inline isize copyBackToContext(Context ctx, FormatArgs fmtArgs, const Span<char> spSrc) noexcept;

inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const StringView& str) noexcept;

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

template<typename PTR_T> requires std::is_pointer_v<PTR_T>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, PTR_T p) noexcept;

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

template<template<typename> typename CON_T, typename T>
inline isize formatToContextExpSize(Context ctx, FormatArgs fmtArgs, const CON_T<T>& x, const isize contSize) noexcept;

template<template<typename, isize> typename CON_T, typename T, isize SIZE>
inline isize formatToContextTemplateSize(Context ctx, FormatArgs fmtArgs, const CON_T<T, SIZE>& x, const isize contSize) noexcept;

template<template<typename> typename CON_T, typename T>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const CON_T<T>& x) noexcept;

template<typename T, isize N>
inline isize formatToContext(Context ctx, FormatArgs fmtArgs, const T (&a)[N]) noexcept;

} /* namespace adt::print */

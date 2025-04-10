#pragma once

#include "StringDecl.hh"

#include <type_traits>

namespace adt::print
{

enum class BASE : u8;

enum class FMT_FLAGS : u8;

struct FormatArgs;
struct Context;

template<typename ...ARGS_T> inline ssize out(const StringView fmt, const ARGS_T&... tArgs) noexcept;
template<typename ...ARGS_T> inline ssize err(const StringView fmt, const ARGS_T&... tArgs) noexcept;

inline ssize printArgs(Context ctx) noexcept;

inline constexpr bool oneOfChars(const char x, const StringView chars) noexcept;

inline ssize parseFormatArg(FormatArgs* pArgs, const StringView fmt, ssize fmtIdx) noexcept;

template<typename INT_T> requires std::is_integral_v<INT_T>
inline constexpr void intToBuffer(INT_T x, Span<char> spBuff, FormatArgs fmtArgs) noexcept;

inline ssize copyBackToContext(Context ctx, FormatArgs fmtArgs, const Span<char> spSrc) noexcept;

inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, const StringView& str) noexcept;

template<int SIZE> requires(SIZE > 1)
inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, const StringFixed<SIZE>& str) noexcept;

inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, const char* str) noexcept;

inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, char* const& pNullTerm) noexcept;

inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, bool b) noexcept;

template<typename INT_T> requires std::is_integral_v<INT_T>
inline constexpr ssize formatToContext(Context ctx, FormatArgs fmtArgs, const INT_T& x) noexcept;

inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, const f32 x) noexcept;

inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, const f64 x) noexcept;

inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, const wchar_t x) noexcept;

inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, const char32_t x) noexcept;

inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, const char x) noexcept;

inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, null) noexcept;

template<typename PTR_T> requires std::is_pointer_v<PTR_T>
inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, PTR_T p) noexcept;

template<typename T, typename ...ARGS_T>
inline constexpr ssize printArgs(Context ctx, const T& tFirst, const ARGS_T&... tArgs) noexcept;

template<ssize SIZE = 512, typename ...ARGS_T>
inline ssize toFILE(FILE* fp, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline constexpr ssize toBuffer(char* pBuff, ssize buffSize, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline constexpr ssize toString(StringView* pDest, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline constexpr ssize toSpan(Span<char> sp, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline ssize out(const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline ssize err(const StringView fmt, const ARGS_T&... tArgs) noexcept;

inline ssize FormatArgsToFmt(const FormatArgs fmtArgs, Span<char> spFmt) noexcept;

template<template<typename> typename CON_T, typename T>
inline ssize formatToContextExpSize(Context ctx, FormatArgs fmtArgs, const CON_T<T>& x, const ssize contSize) noexcept;

template<template<typename, ssize> typename CON_T, typename T, ssize SIZE>
inline ssize formatToContextTemplateSize(Context ctx, FormatArgs fmtArgs, const CON_T<T, SIZE>& x, const ssize contSize) noexcept;

template<template<typename> typename CON_T, typename T>
inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, const CON_T<T>& x) noexcept;

template<typename T, ssize N>
inline ssize formatToContext(Context ctx, FormatArgs fmtArgs, const T (&a)[N]) noexcept;

} /* namespace adt::print */

#pragma once

#include "String.inc"
#include "enum.hh"

#include <type_traits>
#include <cstdio>

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
    Buffer(IAllocator* pAlloc) noexcept : m_pAlloc {pAlloc} {}
    Buffer(IAllocator* pAlloc, isize prealloc);
    Buffer(IAllocator* pAlloc, char* pBuff, isize buffSize) noexcept : m_pAlloc {pAlloc}, m_pData {pBuff}, m_cap {buffSize} {}
    Buffer(char* pBuff, isize buffSize) noexcept : m_pData {pBuff}, m_cap {buffSize} {}

    /* */

    explicit operator StringView() noexcept;
    explicit operator String() noexcept;

    /* */

    isize push(char c) noexcept(false); /* AllocException */
    isize push(const Span<const char> sp) noexcept(false); /* AllocException */
    isize push(const StringView sv) noexcept(false); /* AllocException */
    isize pushN(const char c, const isize nTimes) noexcept(false); /* AllocException */

protected:
    void grow(isize newCap) noexcept(false); /* AllocException */
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

inline const char* shorterSourcePath(const char* ntsSourcePath);

inline isize printArgs(Context ctx);

inline isize parseFormatArg(FormatArgs* pArgs, const StringView fmt, isize fmtIdx) noexcept;

template<typename T>
inline isize intToBuffer(T x, Span<char> spBuff, FormatArgs fmtArgs) noexcept;

inline isize copyBackToContext(Context ctx, FormatArgs fmtArgs, const StringView sv);

inline isize format(Context ctx, FormatArgs fmtArgs, const StringView str);

template<typename STRING_T> requires ConvertsToStringView<STRING_T>
inline isize format(Context ctx, FormatArgs fmtArgs, const STRING_T& str);

inline isize format(Context ctx, FormatArgs fmtArgs, const char* str);

inline isize format(Context ctx, FormatArgs fmtArgs, char* const& pNullTerm);

inline isize format(Context ctx, FormatArgs fmtArgs, bool b);

template<typename T> requires (std::is_integral_v<T>)
inline constexpr isize format(Context ctx, FormatArgs fmtArgs, const T x);

inline isize format(Context ctx, FormatArgs fmtArgs, const f32 x);

inline isize format(Context ctx, FormatArgs fmtArgs, const f64 x);

inline isize format(Context ctx, FormatArgs fmtArgs, const wchar_t x);

inline isize format(Context ctx, FormatArgs fmtArgs, const char32_t x);

inline isize format(Context ctx, FormatArgs fmtArgs, const char x);

inline isize format(Context ctx, FormatArgs fmtArgs, null);

inline isize format(Context ctx, FormatArgs fmtArgs, Empty);

template<typename T>
inline isize format(Context ctx, FormatArgs fmtArgs, const T* const p);

template<typename T, typename ...ARGS_T>
inline constexpr isize printArgs(Context ctx, const T& tFirst, const ARGS_T&... tArgs);

template<isize SIZE = 512, typename ...ARGS_T>
inline isize toFILE(FILE* fp, const StringView fmt, const ARGS_T&... tArgs);

template<isize SIZE = 512, typename ...ARGS_T>
inline isize toFILE(IAllocator* pAlloc, FILE* fp, const StringView fmt, const ARGS_T&... tArgs);

template<typename ...ARGS_T>
inline constexpr isize toBuffer(char* pBuff, isize buffSize, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline constexpr isize toSpan(Span<char> sp, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
[[nodiscard]] inline String toString(IAllocator* pAlloc, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
[[nodiscard]] inline String toString(IAllocator* pAlloc, isize prealloc, const StringView fmt, const ARGS_T&... tArgs) noexcept;

template<typename ...ARGS_T>
inline isize out(const StringView fmt, const ARGS_T&... tArgs);

template<typename ...ARGS_T>
inline isize err(const StringView fmt, const ARGS_T&... tArgs);

inline isize formatExpSize(Context ctx, FormatArgs fmtArgs, const auto& x, const isize contSize);

inline isize formatUntilEnd(Context ctx, FormatArgs fmtArgs, const auto& x);

template<typename ...ARGS>
inline isize formatVariadic(Context ctx, FormatArgs fmtArgs, const ARGS&... args);

template<typename T>
requires (HasSizeMethod<T> && !ConvertsToStringView<T>)
inline isize format(Context ctx, FormatArgs fmtArgs, const T& x);

template<typename T>
requires HasNextIt<T>
inline isize format(Context ctx, FormatArgs fmtArgs, const T& x);

template<typename T, isize N>
inline isize format(Context ctx, FormatArgs fmtArgs, const T (&a)[N]);

template<typename T>
concept Printable = requires(const T& c)
{ print::format({}, {}, c); };

template<typename T>
requires (!Printable<T>)
inline isize format(Context ctx, FormatArgs fmtArgs, const T&);

} /* namespace adt::print */

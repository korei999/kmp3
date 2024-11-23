#pragma once

#ifdef ADT_STD_TYPES
    #include <cstdint>
#endif

namespace adt
{

#ifdef ADT_STD_TYPES

using s8 = ::int8_t;
using u8 = ::uint8_t;
using s16 = ::int16_t;
using u16 = ::uint16_t;
using s32 = ::int32_t;
using u32 = ::uint32_t;
using s64 = ::int64_t;
using u64 = ::uint64_t;
using pdiff = ::ptrdiff_t;

#else

using s8 = signed char;
using u8 = unsigned char;
using s16 = signed short;
using u16 = unsigned short;
using s32 = signed int;
using u32 = unsigned int;
using s64 = signed long long;
using u64 = unsigned long long;
using pdiff = long long;

#endif

using f32 = float;
using f64 = double;

using null = decltype(nullptr);

constexpr u16 NPOS8 = u8(-1);
constexpr u16 NPOS16 = u16(-1);
constexpr u32 NPOS = u32(-1U);
constexpr u64 NPOS64 = u64(-1ULL);

#if defined __clang__ || __GNUC__
    #define ADT_NO_UB __attribute__((no_sanitize("undefined")))
#else
    #define ADT_NO_UB
#endif

} /* namespace adt */

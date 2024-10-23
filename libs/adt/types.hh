#pragma once

#include <stdint.h>

namespace adt
{

using s8 = int8_t;
using u8 = uint8_t;
using s16 = int16_t;
using u16 = uint16_t;
using s32 = int32_t;
using u32 = uint32_t;
using s64 = int64_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using null = decltype(nullptr);

constexpr int U24_MAX = 8388607;

struct u24
{
    u8 data[3];
};

constexpr u16 NPOS8 = 0xff;
constexpr u16 NPOS16 = 0xffff;
constexpr u32 NPOS = u32(-1U);
constexpr u64 NPOS64 = u64(-1UL);

#if defined __clang__ || __GNUC__
    #define ADT_NO_UB __attribute__((no_sanitize("undefined")))
#else
    #define ADT_NO_UB
#endif

} /* namespace adt */

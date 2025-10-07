/*
 * Copyright (c) 2015 Daniel Kirchner
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "Span-inl.hh"
#include "assert.hh"
#include "types.hh"

#ifdef ADT_SSE4_2
    #include <nmmintrin.h>
#endif

namespace adt::hash
{

struct xxh64
{
    static constexpr usize
    hash(const u8* p, usize len, usize seed)
    {
        return finalize((len >= 32 ? h32bytes(p, len, seed) : seed + PRIME5) + len, p + (len & ~0x1F), len & 0x1F);
    }

private:
    static constexpr usize PRIME1 = 11400714785074694791ULL;
    static constexpr usize PRIME2 = 14029467366897019727ULL;
    static constexpr usize PRIME3 = 1609587929392839161ULL;
    static constexpr usize PRIME4 = 9650029242287828579ULL;
    static constexpr usize PRIME5 = 2870177450012600261ULL;

    static constexpr usize
    rotl(usize x, int r)
    {
        return ((x << r) | (x >> (64 - r)));
    }

    static constexpr usize
    mix1(const usize h, const usize prime, int rshift)
    {
        return (h ^ (h >> rshift)) * prime;
    }

    static constexpr usize
    mix2(const usize p, const usize v = 0)
    {
        return rotl(v + p * PRIME2, 31) * PRIME1;
    }

    static constexpr usize
    mix3(const usize h, const usize v)
    {
        return (h ^ mix2(v)) * PRIME1 + PRIME4;
    }
#ifdef XXH64_BIG_ENDIAN
    static constexpr u32
    endian32(const u8* v)
    {
        return u32(u8(v[3])) | (u32(u8(v[2])) << 8) | (u32(u8(v[1])) << 16) | (u32(u8(v[0])) << 24);
    }

    static constexpr usize
    endian64(const u8* v)
    {
        return usize(u8(v[7])) | (usize(u8(v[6])) << 8) | (usize(u8(v[5])) << 16) | (usize(u8(v[4])) << 24) |
            (usize(u8(v[3])) << 32) | (usize(u8(v[2])) << 40) | (usize(u8(v[1])) << 48) | (usize(u8(v[0])) << 56);
    }
#else
    static constexpr u32
    endian32(const u8* v)
    {
        return u32(u8(v[0])) | (u32(u8(v[1])) << 8) | (u32(u8(v[2])) << 16) | (u32(u8(v[3])) << 24);
    }

    static constexpr usize
    endian64(const u8* v)
    {
        return usize(u8(v[0])) | (usize(u8(v[1])) << 8) | (usize(u8(v[2])) << 16) | (usize(u8(v[3])) << 24) |
            (usize(u8(v[4])) << 32) | (usize(u8(v[5])) << 40) | (usize(u8(v[6])) << 48) | (usize(u8(v[7])) << 56);
    }
#endif
    static constexpr usize
    fetch64(const u8* p, const usize v = 0)
    {
        return mix2(endian64(p), v);
    }

    static constexpr usize
    fetch32(const u8* p)
    {
        return usize(endian32(p)) * PRIME1;
    }

    static constexpr usize
    fetch8(const u8* p)
    {
        return u8(*p) * PRIME5;
    }

    static constexpr usize
    finalize(const usize h, const u8* p, usize len)
    {
        return (len >= 8) ? (finalize(rotl(h ^ fetch64(p), 27) * PRIME1 + PRIME4, p + 8, len - 8))
            : ((len >= 4) ? (finalize(rotl(h ^ fetch32(p), 23) * PRIME2 + PRIME3, p + 4, len - 4))
                : ((len > 0) ? (finalize(rotl(h ^ fetch8(p), 11) * PRIME1, p + 1, len - 1))
                    : (mix1(mix1(mix1(h, PRIME2, 33), PRIME3, 29), 1, 32))));
    }
    static constexpr usize
    h32bytes(const u8* p, usize len, const usize v1, const usize v2, const usize v3, const usize v4)
    {
        return (len >= 32)
            ? h32bytes(
                p + 32, len - 32, fetch64(p, v1), fetch64(p + 8, v2), fetch64(p + 16, v3), fetch64(p + 24, v4)
            )
            : mix3(mix3(mix3(mix3(rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18), v1), v2), v3), v4);
    }

    static constexpr usize
    h32bytes(const u8* p, usize len, const usize seed)
    {
        return h32bytes(p, len, seed + PRIME1 + PRIME2, seed + PRIME2, seed, seed - PRIME1);
    }
};

#ifdef ADT_SSE4_2

ADT_NO_UB inline usize
crc32(const u8* p, isize byteSize, usize seed = 0)
{
    usize crc = seed;

    isize i = 0;
    for (; i + 7 < byteSize; i += 8)
        crc = _mm_crc32_u64(crc, *reinterpret_cast<const usize*>(&p[i]));

    if (i < byteSize && byteSize >= 8)
    {
        ADT_ASSERT(byteSize - 8 >= 0, "{}", byteSize - 8);
        crc = _mm_crc32_u64(crc, *reinterpret_cast<const usize*>(&p[byteSize - 8]));
    }
    else
    {
        for (; i + 3 < byteSize; i += 4)
            crc = usize(_mm_crc32_u32(u32(crc), *reinterpret_cast<const u32*>(&p[i])));
        for (; i < byteSize; ++i)
            crc = usize(_mm_crc32_u8(u32(crc), u8(p[i])));
    }

    return ~crc;
}

#endif

template<typename STRING_T>
requires ConvertsToStringView<STRING_T>
ADT_NO_UB inline usize
func(const STRING_T& x)
{
#ifdef ADT_SSE4_2
    return crc32(reinterpret_cast<const u8*>(x.data()), x.size(), 0);
#else
    return xxh64::hash(reinterpret_cast<const u8*>(x.data()), x.size(), 0);
#endif
}

template<typename T>
requires ((sizeof(T) == 8) && !ConvertsToStringView<T>)
ADT_NO_UB inline usize
func(const T& x)
{
#ifdef ADT_SSE4_2
    return _mm_crc32_u64(0, x);
#else
    return xxh64::hash(reinterpret_cast<const u8*>(&x), sizeof(T), 0);
#endif
}

template<typename T>
requires (sizeof(T) == 4)
ADT_NO_UB inline usize
func(const T& x)
{
#ifdef ADT_SSE4_2
    return usize(_mm_crc32_u32(0, x));
#else
    return xxh64::hash(reinterpret_cast<const u8*>(&x), sizeof(T), 0);
#endif
}

template<typename T>
inline usize
func(const T* pBuff, isize byteSize, usize seed = 0)
{
#ifdef ADT_SSE4_2
    return crc32(reinterpret_cast<const u8*>(pBuff), byteSize, seed);
#else
    return xxh64::hash(reinterpret_cast<const u8*>(pBuff), byteSize, seed);
#endif
}

template<typename T>
inline usize
func(const T& x)
{
    return func(reinterpret_cast<const u8*>(&x), sizeof(T), 0);
}

template<isize N>
inline usize
func(const char (&aChars)[N])
{
    /* WARN: string literals include '\0', which completely changes the hash */
    return func(reinterpret_cast<const u8*>(aChars), N - 1, 0);
}

template<typename T>
inline usize
func(const Span<T> sp)
{
    return func(reinterpret_cast<const u8*>(sp.m_pData), sp.m_size * sizeof(T), 0);
}

/* just return the key */
template<typename T>
inline usize
dumbFunc(const T& key)
{
    return static_cast<usize>(key);
}

ADT_NO_UB inline usize
nullTermStringFunc(const char* const& nts)
{
    return func(StringView(nts));
}

template<typename T>
inline constexpr usize
cantorPair(const T& k0, const T& k1)
{
    return (((k0 + k1))/2) * (k0 + k1 + 1) + k1;
}

} /* namespace adt::hash */

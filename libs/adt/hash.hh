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

#include "types.hh"
#include "Span.hh"

namespace adt::hash
{

struct xxh64
{
    static constexpr u64
    hash(const char* p, u64 len, u64 seed)
    {
        return finalize((len >= 32 ? h32bytes(p, len, seed) : seed + PRIME5) + len, p + (len & ~0x1F), len & 0x1F);
    }

private:
    static constexpr u64 PRIME1 = 11400714785074694791ULL;
    static constexpr u64 PRIME2 = 14029467366897019727ULL;
    static constexpr u64 PRIME3 = 1609587929392839161ULL;
    static constexpr u64 PRIME4 = 9650029242287828579ULL;
    static constexpr u64 PRIME5 = 2870177450012600261ULL;

    static constexpr u64
    rotl(u64 x, int r)
    {
        return ((x << r) | (x >> (64 - r)));
    }

    static constexpr u64
    mix1(const u64 h, const u64 prime, int rshift)
    {
        return (h ^ (h >> rshift)) * prime;
    }

    static constexpr u64
    mix2(const u64 p, const u64 v = 0)
    {
        return rotl(v + p * PRIME2, 31) * PRIME1;
    }

    static constexpr u64
    mix3(const u64 h, const u64 v)
    {
        return (h ^ mix2(v)) * PRIME1 + PRIME4;
    }
#ifdef XXH64_BIG_ENDIAN
    static constexpr u32
    endian32(const char* v)
    {
        return u32(u8(v[3])) | (u32(u8(v[2])) << 8) | (u32(u8(v[1])) << 16) | (u32(u8(v[0])) << 24);
    }

    static constexpr u64
    endian64(const char* v)
    {
        return u64(u8(v[7])) | (u64(u8(v[6])) << 8) | (u64(u8(v[5])) << 16) | (u64(u8(v[4])) << 24) |
               (u64(u8(v[3])) << 32) | (u64(u8(v[2])) << 40) | (u64(u8(v[1])) << 48) | (u64(u8(v[0])) << 56);
    }
#else
    static constexpr u32
    endian32(const char* v)
    {
        return u32(u8(v[0])) | (u32(u8(v[1])) << 8) | (u32(u8(v[2])) << 16) | (u32(u8(v[3])) << 24);
    }

    static constexpr u64
    endian64(const char* v)
    {
        return u64(u8(v[0])) | (u64(u8(v[1])) << 8) | (u64(u8(v[2])) << 16) | (u64(u8(v[3])) << 24) |
               (u64(u8(v[4])) << 32) | (u64(u8(v[5])) << 40) | (u64(u8(v[6])) << 48) | (u64(u8(v[7])) << 56);
    }
#endif
    static constexpr u64
    fetch64(const char* p, const u64 v = 0)
    {
        return mix2(endian64(p), v);
    }

    static constexpr u64
    fetch32(const char* p)
    {
        return u64(endian32(p)) * PRIME1;
    }

    static constexpr u64
    fetch8(const char* p)
    {
        return u8(*p) * PRIME5;
    }

    static constexpr u64
    finalize(const u64 h, const char* p, u64 len)
    {
        return (len >= 8) ? (finalize(rotl(h ^ fetch64(p), 27) * PRIME1 + PRIME4, p + 8, len - 8))
                          : ((len >= 4) ? (finalize(rotl(h ^ fetch32(p), 23) * PRIME2 + PRIME3, p + 4, len - 4))
                                        : ((len > 0) ? (finalize(rotl(h ^ fetch8(p), 11) * PRIME1, p + 1, len - 1))
                                                     : (mix1(mix1(mix1(h, PRIME2, 33), PRIME3, 29), 1, 32))));
    }
    static constexpr u64
    h32bytes(const char* p, u64 len, const u64 v1, const u64 v2, const u64 v3, const u64 v4)
    {
        return (len >= 32)
                   ? h32bytes(
                         p + 32, len - 32, fetch64(p, v1), fetch64(p + 8, v2), fetch64(p + 16, v3), fetch64(p + 24, v4)
                     )
                   : mix3(mix3(mix3(mix3(rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18), v1), v2), v3), v4);
    }

    static constexpr u64
    h32bytes(const char* p, u64 len, const u64 seed)
    {
        return h32bytes(p, len, seed + PRIME1 + PRIME2, seed + PRIME2, seed, seed - PRIME1);
    }
};

template<typename T>
inline usize
func(const T* pBuff, ssize byteSize, usize seed = 0)
{
    return xxh64::hash((const char*)pBuff, byteSize, seed);
}

template<typename T>
inline usize
func(const T& x)
{
    return xxh64::hash((const char*)&x, sizeof(T), 0);
}

template<ssize N>
constexpr usize
func(const char (&aChars)[N])
{
    /* WARN: string literals include '\0', which completely changes the hash */
    return xxh64::hash(aChars, N - 1, 0);
}

template<typename T>
inline usize
func(const Span<T> sp)
{
    return xxh64::hash((const char*)sp.data(), sp.size() * sizeof(T), 0);
}

/* just return the key */
template<typename T>
inline usize
dumbFunc(const T& key)
{
    return static_cast<usize>(key);
}

} /* namespace adt::hash */

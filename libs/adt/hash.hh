#pragma once

#include "types.hh"
#include "Span.inc"

#include <nmmintrin.h>

namespace adt::hash
{

inline usize
crc32(const u8* p, isize byteSize, usize seed = 0)
{
    usize crc = seed;

    isize i = 0;
    for (; i + 7 < byteSize; i += 8)
        crc = _mm_crc32_u64(crc, *reinterpret_cast<const usize*>(&p[i]));
    for (; i < byteSize; ++i)
        crc = u64(_mm_crc32_u8(u32(crc), u8(p[i])));

    return ~crc;
}

template<typename T>
inline usize
func(const T* pBuff, isize byteSize, usize seed = 0)
{
    return crc32(reinterpret_cast<const u8*>(pBuff), byteSize, seed);
}

template<typename T>
inline usize
func(const T& x)
{
    return crc32(reinterpret_cast<const u8*>(&x), sizeof(T), 0);
}

template<isize N>
inline usize
func(const char (&aChars)[N])
{
    /* WARN: string literals include '\0', which completely changes the hash */
    return crc32(reinterpret_cast<const u8*>(aChars), N - 1, 0);
}

template<typename T>
inline usize
func(const Span<T> sp)
{
    return crc32(reinterpret_cast<const u8*>(sp.m_pData), sp.m_size * sizeof(T), 0);
}

/* just return the key */
template<typename T>
inline usize
dumbFunc(const T& key)
{
    return static_cast<usize>(key);
}

} /* namespace adt::hash */

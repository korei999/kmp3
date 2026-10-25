#pragma once

#include "common.h"

#include <assert.h>

#ifdef K_SSE4_2
    #include <nmmintrin.h>

/* Not a real crc remainder. Just a hash function using crc32 hardware intrinsics. */
K_NO_UB static inline uint64_t
k_hash_crc32(const uint8_t* p, ssize_t byteSize, uint64_t seed)
{
    uint64_t crc = seed;

    ssize_t i = 0;
    for (; i + 7 < byteSize; i += 8)
        crc = _mm_crc32_u64(crc, *(const uint64_t*)&p[i]);

    if (i < byteSize && byteSize >= 8)
    {
        assert(byteSize - 8 >= 0);
        crc = _mm_crc32_u64(crc, *(const uint64_t*)&p[byteSize - 8]);
    }
    else
    {
        for (; i + 3 < byteSize; i += 4)
            crc = (uint64_t)(_mm_crc32_u32((uint64_t)crc, *(const uint32_t*)&p[i]));
        for (; i < byteSize; ++i)
            crc = (uint64_t)(_mm_crc32_u8((uint32_t)crc, (uint8_t)p[i]));
    }

    return ~crc;
}

#endif

K_NO_UB static inline uint64_t
k_hash_mulXor(const uint8_t* p, ssize_t byteSize, uint64_t seed)
{
    ssize_t i = 0;
    uint64_t hash = seed;
    static const uint64_t randomGiantNumber = 0xf9135213895caf14LLU;

    for (; i < byteSize; ++i)
        hash += (p[i] * randomGiantNumber) ^ p[i];

    return hash;
}

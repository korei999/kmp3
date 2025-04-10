#pragma once

#include "adt/types.hh"

namespace adt::rng
{

struct PCG32
{
    u64 m_state {};
    u64 m_inc {}; /* Must be odd. */

    /* */

    PCG32() = default;

    PCG32(u64 seed, u64 seq = 1) : m_inc((seq << 1u) | 1u)
    {
        next();
        m_state += seed;
        next();
    }

    /* */

    u32
    next()
    {
        u64 oldState = m_state;
        m_state = oldState * 6364136223846793005ULL + m_inc;
        u32 xorShifted = static_cast<u32>(((oldState >> 18u) ^ oldState) >> 27u);
        u32 rot = static_cast<u32>(oldState >> 59u);
        return (xorShifted >> rot) | (xorShifted << ((-rot) & 31));
    }

    u32
    nextInRange(u32 min, u32 max)
    {
        /* Rejection sampling. */
        u32 range = max - min;
        u32 threshold = -range % range;
        while (true)
        {
            u32 r = next();
            if (r >= threshold)
                return min + (r % range);
        }
    }

    bool
    testLuck(const u32 chancePercent)
    {
        return nextInRange(0, 101) >= (100 - chancePercent);
    }
};

} /* namespace adt::rng */

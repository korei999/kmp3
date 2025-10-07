/*
 * PCG Random Number Generation for C++
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *     http://www.pcg-random.org
 */

#pragma once

#include "types.hh"

namespace adt::rng
{

struct PCG32
{
    usize m_state {};
    usize m_inc {}; /* Must be odd. */

    /* */

    PCG32() = default;

    PCG32(usize seed, usize seq = 1) : m_inc((seq << 1u) | 1u)
    {
        next();
        m_state += seed;
        next();
    }

    /* */

    u32
    next()
    {
        usize oldState = m_state;
        m_state = oldState * 6364136223846793005ull + m_inc;
        u32 xorShifted = static_cast<u32>(((oldState >> 18u) ^ oldState) >> 27u);
        u32 rot = static_cast<u32>(oldState >> 59u);
        return (xorShifted >> rot) | (xorShifted << ((-rot) & 31));
    }

    /* Inclusive range. */
    u32
    nextInRange(u32 min, u32 max)
    {
        max += 1;
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
        return nextInRange(0, 100) >= (100 - chancePercent);
    }
};

} /* namespace adt::rng */

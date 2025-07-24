/* https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue */

/*  Multi-producer/multi-consumer bounded queue.
 *  Copyright (c) 2010-2011, Dmitry Vyukov. All rights reserved.
 *  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright notice, this list of
 *        conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright notice, this list
 *        of conditions and the following disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *  THIS SOFTWARE IS PROVIDED BY DMITRY VYUKOV "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *  DMITRY VYUKOV OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  The views and conclusions contained in the software and documentation are those of the authors and should not be interpreted
 *  as representing official policies, either expressed or implied, of Dmitry Vyukov.
 */


#pragma once

#include "Opt.hh"
#include "types.hh"
#include "atomic.hh"

namespace adt
{

template<typename T, int CAP>
struct QueueMPMC
{
    static_assert(CAP >= 0 && isPowerOf2(CAP));

    static constexpr isize CACHELINE_SIZE = 64;
    static constexpr isize BUFFER_MASK = CAP - 1;
    using CacheLinePad = char[CACHELINE_SIZE];

    struct Cell
    {
        atomic::Int sequence {};
        T data {};
    };

    /* */

    CacheLinePad m_pad0 {};
    Cell m_aBuff[CAP] {};
    CacheLinePad m_pad1 {};
    atomic::Int m_enqueuePos {};
    CacheLinePad m_pad2 {};
    atomic::Int m_dequeuePos {};
    CacheLinePad m_pad3 {};

    /* */

    ADT_WARN_INIT QueueMPMC() = default;

    QueueMPMC(InitFlag);

    /* */

    bool push(const T& x);
    [[nodiscard]] Opt<T> pop();
    int cap() const noexcept { return CAP; }
    bool empty() const noexcept;
};

template<typename T, int CAP>
inline
QueueMPMC<T, CAP>::QueueMPMC(InitFlag)
{
    for (int i = 0; i < CAP; ++i)
        m_aBuff[i].sequence = atomic::Int(i);
}

template<typename T, int CAP>
inline bool
QueueMPMC<T, CAP>::push(const T& x)
{
    Cell* pCell;
    int pos = m_enqueuePos.load(atomic::ORDER::RELAXED);

    while (true)
    {
        pCell = &m_aBuff[pos & BUFFER_MASK];
        int seq = pCell->sequence.load(atomic::ORDER::ACQUIRE);
        int diff = seq - pos;
        if (diff == 0)
        {
            if (m_enqueuePos.compareExchangeWeak(&pos, pos + 1,
                    atomic::ORDER::RELAXED, atomic::ORDER::RELAXED
            )
            )
            {
                break;
            }
        }
        else if (diff < 0)
        {
            return false;
        }
        else
        {
            pos = m_enqueuePos.load(atomic::ORDER::RELAXED);
        }
    }

    new(&pCell->data) T(x);
    pCell->sequence.store(pos + 1, atomic::ORDER::RELEASE);

    return true;
}

template<typename T, int CAP>
inline Opt<T>
QueueMPMC<T, CAP>::pop()
{
    Cell* pCell;
    int pos = m_dequeuePos.load(atomic::ORDER::RELAXED);

    while (true)
    {
        pCell = &m_aBuff[pos & BUFFER_MASK];
        int seq = pCell->sequence.load(atomic::ORDER::ACQUIRE);
        int diff = seq - (pos + 1);
        if (diff == 0)
        {
            if (m_dequeuePos.compareExchangeWeak(&pos, pos + 1,
                    atomic::ORDER::RELAXED, atomic::ORDER::RELAXED
                )
            )
            {
                break;
            }
        }
        else if (diff < 0)
        {
            return {};
        }
        else
        {
            pos = m_dequeuePos.load(atomic::ORDER::RELAXED);
        }
    }

    Opt<T> ret = std::move(pCell->data);
    if constexpr (!std::is_trivially_destructible_v<T>)
        pCell->data.~T();
    pCell->sequence.store(pos + BUFFER_MASK + 1, atomic::ORDER::RELEASE);

    return ret;
}

template<typename T, int CAP>
inline bool
QueueMPMC<T, CAP>::empty() const noexcept
{
    return m_dequeuePos.load(atomic::ORDER::ACQUIRE) == m_enqueuePos.load(atomic::ORDER::ACQUIRE);
}

} /* namespace adt */

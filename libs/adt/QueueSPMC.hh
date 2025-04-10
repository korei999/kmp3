#pragma once

#include "atomic.hh"

namespace adt
{

template<typename T, int CAP>
struct QueueSPMC
{
    static_assert(isPowerOf2(CAP), "CAP must be a power of 2");

    /* */

    T m_aData[CAP] {};
    atomic::Int m_atom_headI {};
    atomic::Int m_atom_tailI {};

    /* */

    T& data() noexcept { return m_aData; }
    const T& data() const noexcept { return m_aData; }

    int pushBack(const T& x); /* -1 on false */
    T* popFront(); /* nullptr on empty */

    bool empty() const noexcept;
};

template<typename T, int CAP>
inline int
QueueSPMC<T, CAP>::pushBack(const T& x)
{
    int tailI = m_atom_tailI.load(atomic::ORDER::RELAXED);
    int nextTailI = (tailI + 1) & (CAP - 1); /* power of 2 wrapping */

    if (nextTailI == m_atom_headI.load(atomic::ORDER::ACQUIRE))
        return -1; /* full */

    new(m_aData + tailI) T(x);
    m_atom_tailI.store(nextTailI, atomic::ORDER::RELEASE);

    return tailI;
}

template<typename T, int CAP>
inline T*
QueueSPMC<T, CAP>::popFront()
{
    int headI;

    do
    {
        headI = m_atom_headI.load(atomic::ORDER::ACQUIRE);
        if (headI == m_atom_tailI.load(atomic::ORDER::ACQUIRE))
            return nullptr; /* empty */
    }
    while (!m_atom_headI.compareExchangeWeak(
            &headI,
            (headI + 1) & (CAP - 1),
            atomic::ORDER::RELEASE,
            atomic::ORDER::RELAXED
        )
    );

    return &m_aData[headI];
}

template<typename T, int CAP>
inline bool
QueueSPMC<T, CAP>::empty() const noexcept
{
    return m_atom_headI.load(atomic::ORDER::ACQUIRE) ==
        m_atom_tailI.load(atomic::ORDER::ACQUIRE);
}

} /* namespace adt */

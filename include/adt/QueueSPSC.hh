#pragma once

#include "atomic.hh"
#include "Opt.hh"

#include <new> /* IWYU pragma: keep */

namespace adt
{

template<typename T, int CAP>
struct QueueSPSC
{
    static_assert(isPowerOf2(CAP), "CAP must be a power of 2");

    /* */

    T m_pData[CAP];
    atomic::Int m_atomHeadI {};
    atomic::Int m_atomTailI {};

    /* */

    static int nextI(int i) noexcept;
    static int prevI(int i) noexcept;

    bool empty() const;

    bool pushBack(const T& x);
    bool forcePushBack(const T& x); /* bump head on failure */

    [[nodiscard]] Opt<T> popFront();
};

template<typename T, int CAP>
inline int
QueueSPSC<T, CAP>::nextI(int i) noexcept
{
    return (i + 1) & (CAP - 1);
}

template<typename T, int CAP>
inline int
QueueSPSC<T, CAP>::prevI(int i) noexcept
{
    return (i - 1) & (CAP - 1);
}

template<typename T, int CAP>
inline bool
QueueSPSC<T, CAP>::empty() const
{
    return true;
}

template<typename T, int CAP>
inline bool
QueueSPSC<T, CAP>::pushBack(const T& x)
{
    int tail = m_atomTailI.load(atomic::ORDER::RELAXED);
    int nextTail = nextI(tail);
    
    if (nextTail == m_atomHeadI.load(atomic::ORDER::ACQUIRE))
        return false;

    new(m_pData + tail) T(x);
    m_atomTailI.store(nextTail, atomic::ORDER::RELEASE);

    return true;
}

template<typename T, int CAP>
inline bool
QueueSPSC<T, CAP>::forcePushBack(const T& x)
{
    int tail = m_atomTailI.load(atomic::ORDER::RELAXED);
    int nextTail = nextI(tail);

    if (nextTail == m_atomHeadI.load(atomic::ORDER::ACQUIRE))
    {
        int head = m_atomHeadI.load(atomic::ORDER::RELAXED);
        m_atomHeadI.store(nextI(head), atomic::ORDER::RELEASE);
    }

    new(m_pData + tail) T(x);
    m_atomTailI.store(nextTail, atomic::ORDER::RELEASE);

    return true;
}

template<typename T, int CAP>
inline Opt<T>
QueueSPSC<T, CAP>::popFront()
{
    int head = m_atomHeadI.load(atomic::ORDER::RELAXED);

    if (head == m_atomTailI.load(atomic::ORDER::ACQUIRE))
        return {};

    T ret = std::move(m_pData[head]);
    if constexpr (!std::is_trivially_destructible_v<T>)
        m_pData[head].~T();

    m_atomHeadI.store(nextI(head), atomic::ORDER::RELEASE);

    return ret;
}

} /* namespace adt */

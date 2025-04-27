#pragma once

#include "assert.hh"

#include <utility>

namespace adt
{

template<typename T, ssize CAP>
struct QueueArray
{
    enum PUSH_WAY : u8 { HEAD, TAIL };

    static_assert(isPowerOf2(CAP), "CAP must be a power of two");

    /* */

    T m_aData[CAP] {};
    ssize m_headI {};
    ssize m_tailI {};

    /* */

    T& operator[](ssize i) noexcept             { ADT_ASSERT(i >= 0 && i < CAP, "out of CAP({}), i: {}", CAP, i); return m_aData[i]; }
    const T& operator[](ssize i) const noexcept { ADT_ASSERT(i >= 0 && i < CAP, "out of CAP({}), i: {}", CAP, i); return m_aData[i]; }

    ssize cap() const noexcept;

    bool empty() const noexcept;

    ssize pushBack(const T& x) noexcept; /* Index of inserted element, -1 on failure. */

    ssize pushFront(const T& x) noexcept;

    template<typename ...ARGS>
    ssize emplaceBack(ARGS&&... args) noexcept;

    template<typename ...ARGS>
    ssize emplaceFront(ARGS&&... args) noexcept;

    T* popFront() noexcept;

    T* popBack() noexcept;

    template<PUSH_WAY E_WAY>
    ssize fakePush() noexcept;

protected:
    static ssize nextI(ssize i) noexcept;
    static ssize prevI(ssize i) noexcept;
public:

    /* */

    struct It
    {
        QueueArray* m_p {};
        ssize m_i {};

        /* */

        It(const QueueArray* p, const ssize i) : m_p{const_cast<QueueArray*>(p)}, m_i{i} {}

        /* */

        T& operator*() noexcept { return m_p->operator[](m_i); }
        T* operator->() noexcept { return &m_p->operator[](m_i); }

        It
        operator++() noexcept
        {
            m_i = QueueArray::nextI(m_i);
            return *this;
        };

        It
        operator--() noexcept
        {
            m_i = QueueArray::prevI(m_i);
            return *this;
        };

        friend bool operator==(const It l, const It r) { return l.m_i == r.m_i; }
        friend bool operator!=(const It l, const It r) { return l.m_i != r.m_i; }
    };

    It begin() noexcept { return {this, m_headI}; }
    It end() noexcept { return {this, m_tailI}; }
    const It begin() const noexcept { return {this, m_headI}; }
    const It end() const noexcept { return {this, m_tailI}; }

    It rbegin() noexcept { return {this, prevI(m_tailI)}; }
    It rend() noexcept { return {this, prevI(m_headI)}; }
    const It rbegin() const noexcept { return {this, prevI(m_tailI)}; }
    const It rend() const noexcept { return {this, prevI(m_headI)}; }
};

template<typename T, ssize CAP>
inline bool
QueueArray<T, CAP>::empty() const noexcept
{
    return m_headI == m_tailI;
}

template<typename T, ssize CAP>
inline ssize
QueueArray<T, CAP>::cap() const noexcept
{
    return CAP;
}

template<typename T, ssize CAP>
inline ssize
QueueArray<T, CAP>::pushBack(const T& x) noexcept
{
    ssize i = fakePush<PUSH_WAY::TAIL>();
    if (i >= 0) new(m_aData + i) T(x);

    return i;
}

template<typename T, ssize CAP>
inline ssize
QueueArray<T, CAP>::pushFront(const T& x) noexcept
{
    ssize i = fakePush<PUSH_WAY::HEAD>();
    if (i >= 0) new(m_aData + i) T(x);

    return i;
}

template<typename T, ssize CAP>
template<typename ...ARGS>
inline ssize
QueueArray<T, CAP>::emplaceBack(ARGS&&... args) noexcept
{
    ssize i = fakePush<PUSH_WAY::TAIL>();
    if (i >= 0) new(m_aData + i) T(std::forward<ARGS>(args)...);

    return i;
}

template<typename T, ssize CAP>
template<typename ...ARGS>
inline ssize
QueueArray<T, CAP>::emplaceFront(ARGS&&... args) noexcept
{
    ssize i = fakePush<PUSH_WAY::HEAD>();
    if (i >= 0) new(m_aData + i) T(std::forward<ARGS>(args)...);

    return i;
}

template<typename T, ssize CAP>
template<QueueArray<T, CAP>::PUSH_WAY E_WAY>
inline ssize
QueueArray<T, CAP>::fakePush() noexcept
{
    if constexpr (E_WAY == PUSH_WAY::TAIL)
    {
        ssize nextTailI = nextI(m_tailI);
        if (nextTailI == m_headI) return -1; /* full case */

        ssize prevTailI = m_tailI;
        m_tailI = nextTailI;
        return prevTailI;
    }
    else
    {
        ssize nextHeadI = prevI(m_headI);
        if (nextHeadI == m_tailI) return -1;

        return m_headI = nextHeadI;
    }
}

template<typename T, ssize CAP>
inline T*
QueueArray<T, CAP>::popFront() noexcept
{
    if (empty()) return nullptr;

    ssize tmp = m_headI;
    m_headI = nextI(m_headI);

    return &m_aData[tmp];
}

template<typename T, ssize CAP>
inline T*
QueueArray<T, CAP>::popBack() noexcept
{
    if (empty()) return nullptr;

    m_tailI = prevI(m_tailI);

    return &m_aData[m_tailI];
}

template<typename T, ssize CAP>
inline ssize
QueueArray<T, CAP>::nextI(ssize i) noexcept
{
    return (i + 1) & (CAP - 1);
}

template<typename T, ssize CAP>
inline ssize
QueueArray<T, CAP>::prevI(ssize i) noexcept
{
    return (i - 1) & (CAP - 1);
}

} /* namespace adt */

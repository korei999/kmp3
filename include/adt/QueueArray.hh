#pragma once

#include "assert.hh"

#include <utility>

namespace adt
{

template<typename T, isize CAP>
struct QueueArray
{
    enum PUSH_WAY : u8 { HEAD, TAIL };

    static_assert(isPowerOf2(CAP), "CAP must be a power of two");

    /* */

    T m_aData[CAP];
    isize m_headI {};
    isize m_tailI {};

    /* */

    QueueArray() = default;

    /* */

    T& operator[](isize i) noexcept             { ADT_ASSERT(i >= 0 && i < CAP, "out of CAP({}), i: {}", CAP, i); return m_aData[i]; }
    const T& operator[](isize i) const noexcept { ADT_ASSERT(i >= 0 && i < CAP, "out of CAP({}), i: {}", CAP, i); return m_aData[i]; }

    T* data() noexcept { return m_aData; }
    const T* data() const noexcept { return m_aData; }

    isize cap() const noexcept;

    bool empty() const noexcept;

    isize pushBack(const T& x) noexcept; /* Index of inserted element, -1 on failure. */

    isize pushFront(const T& x) noexcept;

    template<typename ...ARGS>
    isize emplaceBack(ARGS&&... args) noexcept;

    template<typename ...ARGS>
    isize emplaceFront(ARGS&&... args) noexcept;

    T popFront() noexcept;

    T popBack() noexcept;

    template<PUSH_WAY E_WAY>
    isize fakePush() noexcept;

protected:
    static isize nextI(isize i) noexcept;
    static isize prevI(isize i) noexcept;
public:

    /* */

    struct It
    {
        QueueArray* m_p {};
        isize m_i {};

        /* */

        It(const QueueArray* p, const isize i) : m_p{const_cast<QueueArray*>(p)}, m_i{i} {}

        /* */

        T& operator*() noexcept { return m_p->operator[](m_i); }
        T* operator->() noexcept { return &m_p->operator[](m_i); }

        It current() noexcept { return {m_p, m_i}; }
        It next() noexcept { return {m_p, QueueArray::nextI(m_i)}; }

        const It current() const noexcept { return {m_p, m_i}; }
        const It next() const noexcept { return {m_p, QueueArray::nextI(m_i)}; }

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

template<typename T, isize CAP>
inline bool
QueueArray<T, CAP>::empty() const noexcept
{
    return m_headI == m_tailI;
}

template<typename T, isize CAP>
inline isize
QueueArray<T, CAP>::cap() const noexcept
{
    return CAP;
}

template<typename T, isize CAP>
inline isize
QueueArray<T, CAP>::pushBack(const T& x) noexcept
{
    isize i = fakePush<PUSH_WAY::TAIL>();
    if (i >= 0) new(m_aData + i) T(x);

    return i;
}

template<typename T, isize CAP>
inline isize
QueueArray<T, CAP>::pushFront(const T& x) noexcept
{
    isize i = fakePush<PUSH_WAY::HEAD>();
    if (i >= 0) new(m_aData + i) T(x);

    return i;
}

template<typename T, isize CAP>
template<typename ...ARGS>
inline isize
QueueArray<T, CAP>::emplaceBack(ARGS&&... args) noexcept
{
    isize i = fakePush<PUSH_WAY::TAIL>();
    if (i >= 0) new(m_aData + i) T(std::forward<ARGS>(args)...);

    return i;
}

template<typename T, isize CAP>
template<typename ...ARGS>
inline isize
QueueArray<T, CAP>::emplaceFront(ARGS&&... args) noexcept
{
    isize i = fakePush<PUSH_WAY::HEAD>();
    if (i >= 0) new(m_aData + i) T(std::forward<ARGS>(args)...);

    return i;
}

template<typename T, isize CAP>
template<typename QueueArray<T, CAP>::PUSH_WAY E_WAY>
inline isize
QueueArray<T, CAP>::fakePush() noexcept
{
    if constexpr (E_WAY == PUSH_WAY::TAIL)
    {
        isize nextTailI = nextI(m_tailI);
        if (nextTailI == m_headI) return -1; /* full case */

        isize prevTailI = m_tailI;
        m_tailI = nextTailI;
        return prevTailI;
    }
    else
    {
        isize nextHeadI = prevI(m_headI);
        if (nextHeadI == m_tailI) return -1;

        return m_headI = nextHeadI;
    }
}

template<typename T, isize CAP>
inline T
QueueArray<T, CAP>::popFront() noexcept
{
    ADT_ASSERT(!empty(), "");

    isize tmp = m_headI;
    m_headI = nextI(m_headI);

    T ret = std::move(m_aData[tmp]);
    if constexpr (!std::is_trivially_destructible_v<T>)
        m_aData[tmp].~T();
    return ret;
}

template<typename T, isize CAP>
inline T
QueueArray<T, CAP>::popBack() noexcept
{
    ADT_ASSERT(!empty(), "");

    m_tailI = prevI(m_tailI);

    T ret = std::move(m_aData[m_tailI]);
    if constexpr (!std::is_trivially_destructible_v<T>)
        m_aData[m_tailI].~T();
    return ret;
}

template<typename T, isize CAP>
inline isize
QueueArray<T, CAP>::nextI(isize i) noexcept
{
    return (i + 1) & (CAP - 1);
}

template<typename T, isize CAP>
inline isize
QueueArray<T, CAP>::prevI(isize i) noexcept
{
    return (i - 1) & (CAP - 1);
}

} /* namespace adt */

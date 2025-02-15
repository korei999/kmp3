#pragma once

#include "types.hh"
#include "guard.hh"
#include "Arr.hh"

#include <cstdio>
#include <cassert>
#include <limits>
#include <utility>

namespace adt
{

using PoolHnd = ssize;

template<typename T>
struct PoolNode
{
    T data {};
    bool bDeleted {};
};

/* statically allocated reusable resource collection */
template<typename T, ssize CAP>
struct Pool
{
    Arr<PoolNode<T>, CAP> m_aNodes {};
    Arr<PoolHnd, CAP> m_aFreeIdxs {};
    ssize m_nOccupied {};
    Mutex m_mtx;

    /* */

    ADT_WARN_INIT Pool() = default;
    Pool(INIT_FLAG) : m_mtx(MUTEX_TYPE::PLAIN) {}

    /* */

    T& operator[](ssize i)             { return at(i); }
    const T& operator[](ssize i) const { return at(i); }

    ssize firstI() const;
    ssize lastI() const;
    ssize nextI(ssize i) const;
    ssize prevI(ssize i) const;
    ssize idx(const PoolNode<T>* p) const;
    ssize idx(const T* p) const;
    void destroy();
    [[nodiscard]] PoolHnd getHandle();
    [[nodiscard]] PoolHnd push(const T& value);
    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>) [[nodiscard]] PoolHnd emplace(ARGS&&... args);
    void giveBack(PoolHnd hnd);
    void giveBack(T* hnd);
    ssize getCap() const { return CAP; }
    ssize getSize() const { return m_nOccupied; }
    bool empty() const { return getSize() == 0; }

    /* */

private:
    T& at(ssize i);

    /* */

public:
    struct It
    {
        Pool* s {};
        ssize i {};

        /* */

        It(const Pool* _self, ssize _i) : s(const_cast<Pool*>(_self)), i(_i) {}

        /* */

        auto& operator*() { return s->m_aNodes[i].data; }
        auto* operator->() { return &s->m_aNodes[i].data; }

        It
        operator++()
        {
            i = s->nextI(i);
            return {s, i};
        }

        It
        operator++(int)
        {
            ssize tmp = i;
            i = s->nextI(i);
            return {s, tmp};
        }

        It
        operator--()
        {
            i = s->prevI(i);
            return {s, i};
        }

        It
        operator--(int)
        {
            ssize tmp = i;
            i = s->prevI(i);
            return {s, tmp};
        }

        friend bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend bool operator!=(const It& l, const It& r) { return l.i != r.i; }
    };

    It begin() { return {this, firstI()}; }
    It end() { return {this, getSize() == 0 ? -1 : lastI() + 1}; }

    const It begin() const { return {this, firstI()}; }
    const It end() const { return {this, getSize() == 0 ? -1 : lastI() + 1}; }
};

template<typename T, ssize CAP>
inline ssize
Pool<T, CAP>::firstI() const
{
    if (m_aNodes.m_size == 0) return -1;

    for (ssize i = 0; i < m_aNodes.m_size; ++i)
        if (!m_aNodes[i].bDeleted) return i;

    return m_aNodes.m_size;
}

template<typename T, ssize CAP>
inline ssize
Pool<T, CAP>::lastI() const
{
    if (m_aNodes.m_size == 0) return -1;

    for (ssize i = ssize(m_aNodes.m_size) - 1; i >= 0; --i)
        if (!m_aNodes[i].bDeleted) return i;

    return m_aNodes.m_size;
}

template<typename T, ssize CAP>
inline ssize
Pool<T, CAP>::nextI(ssize i) const
{
    do ++i;
    while (i < m_aNodes.m_size && m_aNodes[i].bDeleted);

    return i;
}

template<typename T, ssize CAP>
inline ssize
Pool<T, CAP>::prevI(ssize i) const
{
    do --i;
    while (i >= 0 && m_aNodes[i].bDeleted);

    return i;
}

template<typename T, ssize CAP>
inline ssize
Pool<T, CAP>::idx(const PoolNode<T>* p) const
{
    ssize r = p - &m_aNodes[0];
    assert(r < CAP && "[Pool]: out of range");
    return r;
}

template<typename T, ssize CAP>
inline ssize
Pool<T, CAP>::idx(const T* p) const
{
    return (PoolNode<T>*)p - &m_aNodes.m_aData[0];
}

template<typename T, ssize CAP>
inline void
Pool<T, CAP>::destroy()
{
    m_mtx.destroy();
}

template<typename T, ssize CAP>
inline PoolHnd
Pool<T, CAP>::getHandle()
{
    guard::Mtx lock(&m_mtx);

    PoolHnd ret = std::numeric_limits<PoolHnd>::max();

    if (m_nOccupied >= CAP)
    {
#ifndef NDEBUG
        fputs("[MemPool]: no free element, returning NPOS", stderr);
#endif
        return ret;
    }

    ++m_nOccupied;

    if (m_aFreeIdxs.m_size > 0) ret = *m_aFreeIdxs.pop();
    else ret = m_aNodes.fakePush();

    m_aNodes[ret].bDeleted = false;
    return ret;
}

template<typename T, ssize CAP>
inline PoolHnd
Pool<T, CAP>::push(const T& value)
{
    auto idx = getHandle();
    new(&operator[](idx)) T(value);
    return idx;
}

template<typename T, ssize CAP>
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline PoolHnd
Pool<T, CAP>::emplace(ARGS&&... args)
{
    auto idx = getHandle();
    new(&operator[](idx)) T(std::forward<ARGS>(args)...);
    return idx;
}

template<typename T, ssize CAP>
inline void
Pool<T, CAP>::giveBack(PoolHnd hnd)
{
    guard::Mtx lock(&m_mtx);

    --m_nOccupied;
    assert(m_nOccupied < CAP && "[Pool]: nothing to return");

    if (hnd == m_aNodes.getSize() - 1) m_aNodes.fakePop();
    else
    {
        m_aFreeIdxs.push(hnd);
        auto& node = m_aNodes[hnd];
        assert(!node.bDeleted && "[Pool]: returning already deleted node");
        node.bDeleted = true;
    }
}

template<typename T, ssize CAP>
inline void
Pool<T, CAP>::giveBack(T* hnd)
{
    giveBack(idx(hnd));
}

template<typename T, ssize CAP>
inline T&
Pool<T, CAP>::at(ssize i)
{
    ADT_ASSERT(i >= 0 && i < m_aNodes.getSize(), "i: %lld, size: %lld", i, m_aNodes.getSize());
    ADT_ASSERT(!m_aNodes[i].bDeleted, "trying to access deleted node");
    return m_aNodes[i].data;
}

namespace print
{

template<typename T, ssize CAP>
inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const Pool<T, CAP>& x)
{
    return print::formatToContextTemplSize(ctx, fmtArgs, x, x.getSize());
}

} /* namespace print */

} /* namespace adt */

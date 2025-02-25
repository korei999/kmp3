#pragma once

#include "Array.hh"

#include <cstdio>
#include <cassert>

namespace adt
{

template<typename T>
struct PoolHandle
{
    using ResourceType = T;

    /* */

    ssize i = NPOS; /* index of m_aNodes */

    /* */

    explicit operator bool() const { return i != -1; }
};

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
    Array<PoolNode<T>, CAP> m_aNodes {};
    Array<PoolHandle<T>, CAP> m_aFreeIdxs {};
    ssize m_nOccupied {};

    /* */

    ADT_WARN_INIT Pool() = default;
    explicit Pool(InitFlag);

    /* */

    T& operator[](PoolHandle<T> h)             { return at(h); }
    const T& operator[](PoolHandle<T> h) const { return at(h); }

    ssize firstI() const;
    ssize lastI() const;
    ssize nextI(ssize i) const;
    ssize prevI(ssize i) const;

    ssize idx(const PoolNode<T>* p) const;
    ssize idx(const T* p) const;

    [[nodiscard]] PoolHandle<T> getHandle();
    [[nodiscard]] PoolHandle<T> push(const T& value); /* push resource and get handle back */

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    [[nodiscard]] PoolHandle<T> emplace(ARGS&&... args);

    void giveBack(PoolHandle<T> hnd);
    void giveBack(T* hnd);

    ssize cap() const { return CAP; }
    ssize size() const { return m_nOccupied; }

    bool empty() const { return size() == 0; }

    /* */

private:
    T& at(PoolHandle<T> h);

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
    It end() { return {this, size() == 0 ? -1 : lastI() + 1}; }

    const It begin() const { return {this, firstI()}; }
    const It end() const { return {this, size() == 0 ? -1 : lastI() + 1}; }
};

template<typename T, ssize CAP>
Pool<T, CAP>::Pool(InitFlag)
{
    for (ssize i = 0; i < CAP; ++i)
        m_aNodes.m_aData[i].bDeleted = true;
}

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
inline PoolHandle<T>
Pool<T, CAP>::getHandle()
{
    PoolHandle<T> ret {.i = -1};

    if (m_nOccupied >= CAP)
    {
#ifndef NDEBUG
        fputs("[MemPool]: no free element, returning -1", stderr);
#endif
        return {-1};
    }

    ++m_nOccupied;

    if (m_aFreeIdxs.m_size > 0)
        ret = *m_aFreeIdxs.pop();
    else ret.i = m_aNodes.fakePush();

    m_aNodes[ret.i].bDeleted = false;
    return ret;
}

template<typename T, ssize CAP>
inline PoolHandle<T>
Pool<T, CAP>::push(const T& value)
{
    auto idx = getHandle();
    new(&operator[](idx)) T(value);
    return idx;
}

template<typename T, ssize CAP>
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline PoolHandle<T>
Pool<T, CAP>::emplace(ARGS&&... args)
{
    auto idx = getHandle();
    new(&operator[](idx)) T(std::forward<ARGS>(args)...);
    return idx;
}

template<typename T, ssize CAP>
inline void
Pool<T, CAP>::giveBack(PoolHandle<T> hnd)
{
    ADT_ASSERT(m_nOccupied > 0, "nothing to return");
    --m_nOccupied;

    m_aFreeIdxs.push(hnd);
    auto& node = m_aNodes[hnd.i];
    ADT_ASSERT(!node.bDeleted, "returning already deleted node");
    node.bDeleted = true;
}

template<typename T, ssize CAP>
inline void
Pool<T, CAP>::giveBack(T* hnd)
{
    giveBack(PoolHandle<T>(idx(hnd)));
}

template<typename T, ssize CAP>
inline T&
Pool<T, CAP>::at(PoolHandle<T> h)
{
    ADT_ASSERT(h.i >= 0 && h.i < m_aNodes.size(), "i: %lld, size: %lld", h.i, m_aNodes.size());
    ADT_ASSERT(!m_aNodes[h.i].bDeleted, "trying to access deleted node");
    return m_aNodes[h.i].data;
}

namespace print
{

template<typename T, ssize CAP>
inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const Pool<T, CAP>& x)
{
    return print::formatToContextTemplateSize(ctx, fmtArgs, x, x.size());
}

template<typename T>
inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const PoolHandle<T>& x)
{
    return formatToContext(ctx, fmtArgs, x.i);
}

} /* namespace print */

} /* namespace adt */

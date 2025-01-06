#pragma once

#include "IAllocator.hh"
#include "print.hh"

#include <cassert>
#include <new> /* IWYU pragma: keep */

namespace adt
{

#define ADT_QUEUE_FOREACH_I(Q, I) for (int I = (Q)->first, __t = 0; __t < (Q)->size; I = (Q)->nextI(I), __t++)
#define ADT_QUEUE_FOREACH_I_REV(Q, I) for (int I = (Q)->lastI(), __t = 0; __t < (Q)->size; I = (Q)->prevI(I), __t++)

template<typename T>
struct QueueBase
{
    T* m_pData {};
    int m_size {};
    int m_cap {};
    int m_first {};
    int m_last {};

    /* */

    QueueBase() = default;
    QueueBase(IAllocator* pAlloc, ssize prealloc = SIZE_MIN)
        : m_pData {(T*)pAlloc->malloc(prealloc, sizeof(T))},
          m_cap (prealloc) {}

    /* */

    T& operator[](int i)             { assert(i < m_cap && "[Queue]: out of capacity"); return m_pData[i]; }
    const T& operator[](int i) const { assert(i < m_cap && "[Queue]: out of capacity"); return m_pData[i]; }

    [[nodiscard]] int nextI(int i) const { return (i + 1) >= m_cap ? 0 : (i + 1); }
    [[nodiscard]] int prevI(int i) const { return (i - 1) < 0 ? m_cap - 1 : (i - 1); }
    [[nodiscard]] int firstI() const { return empty() ? -1 : m_first; }
    [[nodiscard]] int lastI() const { return empty() ? 0 : m_last - 1; }

    bool empty() const { return m_size == 0; }
    T* data() { return m_pData; }
    const T* data() const { return m_pData; }
    void destroy(IAllocator* p);
    T* pushFront(IAllocator* p, const T& val);
    T* pushBack(IAllocator* p, const T& val);
    void grow(IAllocator* p, ssize size);
    T* popFront();
    T* popBack();
    ssize idx(const T* pItem) const;

    /* */

    struct It
    {
        const QueueBase* s = nullptr;
        int i = 0;
        int counter = 0; /* inc each iteration */

        It(const QueueBase* _s, int _i, int _counter) : s(_s), i(_i), counter(_counter) {}

        T& operator*() const { return s->m_pData[i]; }
        T* operator->() const { return &s->m_pData[i]; }

        It
        operator++()
        {
            i = s->nextI(i);
            counter++;
            return {s, i, counter};
        }

        It operator++(int) { It tmp = *this; ++(*this); return tmp; }

        It
        operator--()
        {
            i = s->prevI(i);
            counter++;
            return {s, i, counter};
        }

        It operator--(int) { It tmp = *this; --(*this); return tmp; }

        friend bool operator==(const It& l, const It& r) { return l.counter == r.counter; }
        friend bool operator!=(const It& l, const It& r) { return l.counter != r.counter; }
    };

    It begin() { return {this, firstI(), 0}; }
    It end() { return {this, {}, m_size}; }
    It rbegin() { return {this, lastI(), 0}; }
    It rend() { return {this, {}, m_size}; }

    const It begin() const { return {this, firstI(), 0}; }
    const It end() const { return {this, {}, m_size}; }
    const It rbegin() const { return {this, lastI(), 0}; }
    const It rend() const { return {this, {}, m_size}; }
};

template<typename T>
inline void
QueueBase<T>::destroy(IAllocator* p)
{
    p->free(m_pData);
}

template<typename T>
inline T*
QueueBase<T>::pushFront(IAllocator* p, const T& val)
{
    if (m_size >= m_cap) grow(p, m_cap * 2);

    int i = m_first;
    int ni = prevI(i);

    new(m_pData + ni) T(val);

    m_first = ni;
    m_size++;

    return &m_pData[ni];
}

template<typename T>
inline T*
QueueBase<T>::pushBack(IAllocator* p, const T& val)
{
    if (m_size >= m_cap) grow(p, m_cap * 2);

    int i = m_last;
    int ni = nextI(i);

    new(m_pData + i) T(val);

    m_last = ni;
    m_size++;

    return &m_pData[i];
}

template<typename T>
inline void
QueueBase<T>::grow(IAllocator* p, ssize size)
{
    auto nQ = QueueBase<T>(p, size);

    for (auto& e : *this) nQ.pushBack(p, e);

    p->free(m_pData);
    *this = nQ;
}

template<typename T>
inline T*
QueueBase<T>::popFront()
{
    assert(m_size > 0);

    T* ret = &m_pData[m_first];
    m_first = nextI(m_first);
    m_size--;

    return ret;
}

template<typename T>
inline T*
QueueBase<T>::popBack()
{
    assert(m_size > 0);

    T* ret = &m_pData[lastI()];
    m_last = prevI(lastI());
    m_size--;

    return ret;
}

template<typename T>
inline ssize
QueueBase<T>::idx(const T* pItem) const
{
    auto r = pItem - m_pData;
    return r;
}

template<typename T>
struct Queue
{
    QueueBase<T> base {};

    /* */

    IAllocator* m_pAlloc {};

    /* */

    Queue() = default;
    Queue(IAllocator* p, ssize prealloc = SIZE_MIN)
        : base(p, prealloc), m_pAlloc(p) {}

    /* */

    T& operator[](ssize i) { return base[i]; }
    const T& operator[](ssize i) const { return base[i]; }

    bool empty() const { return base.empty(); }
    T* data() { return base.data(); }
    const T* data() const { return base.data(); }
    void destroy() { base.destroy(m_pAlloc); }
    T* pushFront(const T& val) { return base.pushFront(m_pAlloc, val); }
    T* pushBack(const T& val) { return base.pushBack(m_pAlloc, val); }
    void resize(ssize size) { base.grow(m_pAlloc, size); }
    T* popFront() { return base.popFront(); }
    T* popBack() { return base.popBack(); }
    ssize idx(const T* pItem) { return base.idx(pItem); }

    /* */

    QueueBase<T>::It begin() { return base.begin(); }
    QueueBase<T>::It end() { return base.end(); }
    QueueBase<T>::It rbegin() { return base.rbegin(); }
    QueueBase<T>::It rend() { return base.rend(); }

    const QueueBase<T>::It begin() const { return base.begin(); }
    const QueueBase<T>::It end() const { return base.end(); }
    const QueueBase<T>::It rbegin() const { return base.rbegin(); }
    const QueueBase<T>::It rend() const { return base.rend(); }
};

namespace print
{

template<typename T>
inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const QueueBase<T>& x)
{
    if (x.empty())
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "(empty)");
    }

    char aBuff[1024] {};
    ssize nRead = 0;
    for (const auto& it : x)
    {
        const char* fmt = (x.idx(&it) == x.m_last - 1U ? "{}" : "{}, ");

        nRead += toBuffer(aBuff + nRead, utils::size(aBuff) - nRead, fmt, it);
    }

    return print::copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

template<typename T>
inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const Queue<T>& x)
{
    return formatToContext(ctx, fmtArgs, x.base);
}

} /* namespace print */

} /* namespace adt */

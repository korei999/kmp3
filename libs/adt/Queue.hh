#pragma once

#include "IAllocator.hh"
#include "print.hh"

#include <cassert>

namespace adt
{

#define ADT_QUEUE_FOREACH_I(Q, I) for (int I = (Q)->first, __t = 0; __t < (Q)->size; I = (Q)->nextI(I), __t++)
#define ADT_QUEUE_FOREACH_I_REV(Q, I) for (int I = (Q)->lastI(), __t = 0; __t < (Q)->size; I = (Q)->prevI(I), __t++)

template<typename T>
struct QueueBase
{
    T* pData {};
    int size {};
    int cap {};
    int first {};
    int last {};

    QueueBase() = default;
    QueueBase(IAllocator* pAlloc, u32 prealloc = SIZE_MIN)
        : pData {(T*)pAlloc->alloc(prealloc, sizeof(T))},
          cap (prealloc) {}

    T& operator[](int i)             { assert(i < cap && "[Queue]: out of capacity"); return pData[i]; }
    const T& operator[](int i) const { assert(i < cap && "[Queue]: out of capacity"); return pData[i]; }

    [[nodiscard]] inline int nextI(int i) const { return (i + 1) >= this->cap ? 0 : (i + 1); }
    [[nodiscard]] inline int prevI(int i) const { return (i - 1) < 0 ? this->cap - 1 : (i - 1); }
    [[nodiscard]] inline int firstI() const { return utils::empty(this) ? -1 : this->first; }
    [[nodiscard]] inline int lastI() const { return utils::empty(this) ? 0 : this->last - 1; }

    void destroy(IAllocator* p);

    T* pushFront(IAllocator* p, const T& val);

    T* pushBack(IAllocator* p, const T& val);

    void resize(IAllocator* p, u32 size);

    T* popFront();

    T* popBack();

    u32 idx(const T* pItem) const;

    struct It
    {
        const QueueBase* s = nullptr;
        int i = 0;
        int counter = 0; /* inc each iteration */

        It(const QueueBase* _s, int _i, int _counter) : s(_s), i(_i), counter(_counter) {}

        T& operator*() const { return s->pData[i]; }
        T* operator->() const { return &s->pData[i]; }

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

    It begin() { return {this, this->firstI(), 0}; }
    It end() { return {this, {}, this->size}; }
    It rbegin() { return {this, this->lastI(), 0}; }
    It rend() { return {this, {}, this->size}; }

    const It begin() const { return {this, this->firstI(), 0}; }
    const It end() const { return {this, {}, this->size}; }
    const It rbegin() const { return {this, this->lastI(), 0}; }
    const It rend() const { return {this, {}, this->size}; }
};

template<typename T>
inline void
QueueBase<T>::destroy(IAllocator* p)
{
    p->free(this->pData);
}

template<typename T>
inline T*
QueueBase<T>::pushFront(IAllocator* p, const T& val)
{
    if (this->size >= this->cap) this->resize(p, this->cap * 2);

    int i = this->first;
    int ni = this->prevI(i);
    this->pData[ni] = val;
    this->first = ni;
    this->size++;

    return &this->pData[ni];
}

template<typename T>
inline T*
QueueBase<T>::pushBack(IAllocator* p, const T& val)
{
    if (this->size >= this->cap) this->resize(p, this->cap * 2);

    int i = this->last;
    int ni = this->nextI(i);
    this->pData[i] = val;
    this->last = ni;
    this->size++;

    return &this->pData[i];
}

template<typename T>
inline void
QueueBase<T>::resize(IAllocator* p, u32 size)
{
    auto nQ = QueueBase<T>(p, size);

    for (auto& e : *this) nQ.pushBack(p, e);

    p->free(this->pData);
    *this = nQ;
}

template<typename T>
inline T*
QueueBase<T>::popFront()
{
    assert(this->size > 0);

    T* ret = &this->pData[this->first];
    this->first = this->nextI(this->first);
    this->size--;

    return ret;
}

template<typename T>
inline T*
QueueBase<T>::popBack()
{
    assert(this->size > 0);

    T* ret = &this->pData[this->lastI()];
    this->last = this->prevI(this->lastI());
    this->size--;

    return ret;
}

template<typename T>
inline u32
QueueBase<T>::idx(const T* pItem) const
{
    return pItem - this->pData;
}

template<typename T>
struct Queue
{
    QueueBase<T> base {};
    IAllocator* pAlloc {};

    Queue() = default;
    Queue(IAllocator* p, u32 prealloc = SIZE_MIN)
        : base(p, prealloc), pAlloc(p) {}

    T& operator[](u32 i) { return base[i]; }
    const T& operator[](u32 i) const { return base[i]; }

    QueueBase<T>::It begin() { return base.begin(); }
    QueueBase<T>::It end() { return base.end(); }
    QueueBase<T>::It rbegin() { return base.rbegin(); }
    QueueBase<T>::It rend() { return rend(); }

    const QueueBase<T>::It begin() const { return base.begin(); }
    const QueueBase<T>::It end() const { return base.end(); }
    const QueueBase<T>::It rbegin() const { return base.rbegin(); }
    const QueueBase<T>::It rend() const { return base.rend(); }

    void destroy() { base.destroy(pAlloc); }

    T* pushFront(const T& val) { return base.pushFront(pAlloc, val); }

    T* pushBack(const T& val) { return base.pushBack(pAlloc, val); }

    void resize(u32 size) { base.resize(pAlloc, size); }

    T* popFront() { return base.popFront(); }

    T* popBack() { return base.popBack(); }

    u32 idx(const T* pItem) { return base.idx(pItem); }
};

namespace utils
{

template<typename T> [[nodiscard]] inline bool empty(const QueueBase<T>* s) { return s->size == 0; }
template<typename T> [[nodiscard]] inline bool empty(const Queue<T>* s) { return empty(&s->base); }

} /* namespace utils */

namespace print
{

template<typename T>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const QueueBase<T>& x)
{
    if (utils::empty(&x))
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "(empty)");
    }

    char aBuff[1024] {};
    u32 nRead = 0;
    for (const auto& it : x)
    {
        const char* fmt;
        if constexpr (std::is_floating_point_v<T>) fmt = (x.idx(&it) == x.last - 1U ? "{:.3}" : "{:.3}, ");
        else fmt = (x.idx(&it) == x.last - 1U ? "{}" : "{}, ");

        nRead += toBuffer(aBuff + nRead, utils::size(aBuff) - nRead, fmt, it);
    }

    return print::copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

template<typename T>
inline u32
formatToContext(Context ctx, FormatArgs fmtArgs, const Queue<T>& x)
{
    return formatToContext(ctx, fmtArgs, x.base);
}

} /* namespace print */

} /* namespace adt */

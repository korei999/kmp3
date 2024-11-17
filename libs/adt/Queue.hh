#pragma once

#include "Allocator.hh"
#include "print.hh"

#include <cassert>

namespace adt
{

#define ADT_QUEUE_FOREACH_I(Q, I) for (int I = (Q)->first, __t = 0; __t < (Q)->size; I = QueueNextI(Q, I), __t++)
#define ADT_QUEUE_FOREACH_I_REV(Q, I) for (int I = QueueLastI(Q), __t = 0; __t < (Q)->size; I = QueuePrevI(Q, I), __t++)

template<typename T> struct QueueBase;

template<typename T> inline T* QueuePushFront(QueueBase<T>* s, Allocator* p, const T& val);
template<typename T> inline T* QueuePushBack(QueueBase<T>* s, Allocator* p, const T& val);
template<typename T> inline void QueueResize(QueueBase<T>* s, Allocator* p, u32 size);
template<typename T> inline T* QueuePopFront(QueueBase<T>* s);
template<typename T> inline T* QueuePopBack(QueueBase<T>* s);

template<typename T>
struct QueueBase
{
    T* pData {};
    int size {};
    int cap {};
    int first {};
    int last {};

    QueueBase() = default;
    QueueBase(Allocator* p, u32 prealloc = SIZE_MIN)
        : pData {(T*)alloc(p, prealloc, sizeof(T))},
          cap (prealloc) {}

    T& operator[](int i)             { assert(i < cap && "[Queue]: out of capacity"); return pData[i]; }
    const T& operator[](int i) const { assert(i < cap && "[Queue]: out of capacity"); return pData[i]; }

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
            i = QueueNextI(s, i);
            counter++;
            return {s, i, counter};
        }

        It operator++(int) { It tmp = *this; ++(*this); return tmp; }

        It
        operator--()
        {
            i = QueuePrevI(s, i);
            counter++;
            return {s, i, counter};
        }

        It operator--(int) { It tmp = *this; --(*this); return tmp; }

        friend bool operator==(const It& l, const It& r) { return l.counter == r.counter; }
        friend bool operator!=(const It& l, const It& r) { return l.counter != r.counter; }
    };

    It begin() { return {this, QueueFirstI(this), 0}; }
    It end() { return {this, {}, this->size}; }
    It rbegin() { return {this, QueueLastI(this), 0}; }
    It rend() { return {this, {}, this->size}; }

    const It begin() const { return {this, QueueFirstI(this), 0}; }
    const It end() const { return {this, {}, this->size}; }
    const It rbegin() const { return {this, QueueLastI(this), 0}; }
    const It rend() const { return {this, {}, this->size}; }
};

template<typename T>
inline void
QueueDestroy(QueueBase<T>*s, Allocator* p)
{
    free(p, s->pData);
}

template<typename T>
inline T*
QueuePushFront(QueueBase<T>* s, Allocator* p, const T& val)
{
    if (s->size >= s->cap) QueueResize(s, p, s->cap * 2);

    int i = s->first;
    int ni = QueuePrevI(s, i);
    s->pData[ni] = val;
    s->first = ni;
    s->size++;

    return &s->pData[ni];
}

template<typename T>
inline T*
QueuePushBack(QueueBase<T>* s, Allocator* p, const T& val)
{
    if (s->size >= s->cap) QueueResize(s, p, s->cap * 2);

    int i = s->last;
    int ni = QueueNextI(s, i);
    s->pData[i] = val;
    s->last = ni;
    s->size++;

    return &s->pData[i];
}

template<typename T>
inline void
QueueResize(QueueBase<T>* s, Allocator* p, u32 size)
{
    auto nQ = QueueBase<T>(p, size);

    for (auto& e : *s) QueuePushBack(&nQ, p, e);

    free(p, s->pData);
    *s = nQ;
}

template<typename T>
inline T*
QueuePopFront(QueueBase<T>* s)
{
    assert(s->size > 0);

    T* ret = &s->pData[s->first];
    s->first = QueueNextI(s, s->first);
    s->size--;

    return ret;
}

template<typename T>
inline T*
QueuePopBack(QueueBase<T>* s)
{
    assert(s->size > 0);

    T* ret = &s->pData[QueueLastI(s)];
    s->last = QueuePrevI(s, QueueLastI(s));
    s->size--;

    return ret;
}

template<typename T>
inline u32
QueueIdx(const QueueBase<T>* s, const T* pItem)
{
    return pItem - s->pData;
}

template<typename T>
struct Queue
{
    QueueBase<T> base {};
    Allocator* pAlloc {};

    Queue() = default;
    Queue(Allocator* p, u32 prealloc = SIZE_MIN)
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
};

template<typename T> inline void QueueDestroy(Queue<T>*s) { QueueDestroy(&s->base, s->pAlloc); }
template<typename T> inline T* QueuePushFront(Queue<T>* s, const T& val) { return QueuePushFront(&s->base, s->pAlloc, val); }
template<typename T> inline T* QueuePushBack(Queue<T>* s, const T& val) { return QueuePushBack(&s->base, s->pAlloc, val); }
template<typename T> inline void QueueResize(Queue<T>* s, u32 size) { QueueResize(&s->base, s->pAlloc, size); }
template<typename T> inline T* QueuePopFront(Queue<T>* s) { return QueuePopFront(&s->base); }
template<typename T> inline T* QueuePopBack(Queue<T>* s) { return QueuePopBack(&s->base); }
template<typename T> inline u32 QueueIdx(const Queue<T>* s, const T* pItem) { return QueueIdx(&s->base, pItem); }

namespace utils
{

template<typename T> [[nodiscard]] inline bool empty(const QueueBase<T>* s) { return s->size == 0; }
template<typename T> [[nodiscard]] inline bool empty(const Queue<T>* s) { return empty(&s->base); }

} /* namespace utils */

template<typename T> [[nodiscard]] inline int QueueNextI(const QueueBase<T>*s, int i) { return (i + 1) >= s->cap ? 0 : (i + 1); }
template<typename T> [[nodiscard]] inline int QueuePrevI(const QueueBase<T>* s, int i) { return (i - 1) < 0 ? s->cap - 1 : (i - 1); }
template<typename T> [[nodiscard]] inline int QueueFirstI(const QueueBase<T>* s) { return utils::empty(s) ? -1 : s->first; }
template<typename T> [[nodiscard]] inline int QueueLastI(const QueueBase<T>* s) { return utils::empty(s) ? 0 : s->last - 1; }

template<typename T> [[nodiscard]] inline int QueueNextI(const Queue<T>*s, int i) { return QueueNextI(&s->base, i); }
template<typename T> [[nodiscard]] inline int QueuePrevI(const Queue<T>* s, int i) { return QueuePrevI(&s->base, i); }
template<typename T> [[nodiscard]] inline int QueueFirstI(const Queue<T>* s) { return QueueFirstI(&s->base); }
template<typename T> [[nodiscard]] inline int QueueLastI(const Queue<T>* s) { return QueueLastI(&s->base); }

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
        if constexpr (std::is_floating_point_v<T>) fmt = (QueueIdx(&x, &it) == x.last - 1 ? "{:.3}" : "{:.3}, ");
        else fmt = (QueueIdx(&x, &it) == x.last - 1 ? "{}" : "{}, ");

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

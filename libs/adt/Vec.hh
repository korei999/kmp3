#pragma once

#include "IAllocator.hh"
#include "utils.hh"
#include "print.hh"

#include <cassert>

namespace adt
{

#define ADT_VEC_FOREACH_I(A, I) for (u32 I = 0; I < (A)->size; ++I)
#define ADT_VEC_FOREACH_I_REV(A, I) for (u32 I = (A)->size - 1; I != -1U ; --I)

/* Dynamic array (aka Vector) */
template<typename T>
struct VecBase
{
    T* pData = nullptr;
    u32 size = 0;
    u32 capacity = 0;

    VecBase() = default;
    VecBase(IAllocator* p, u32 prealloc = 1)
        : pData((T*)p->alloc(prealloc, sizeof(T))),
          size(0),
          capacity(prealloc) {}

    T& operator[](u32 i)             { assert(i < size && "[Vec] out of size"); return pData[i]; }
    const T& operator[](u32 i) const { assert(i < size && "[Vec] out of size"); return pData[i]; }

    u32 push(IAllocator* p, const T& data);

    [[nodiscard]] inline T& last();

    [[nodiscard]] inline const T& last() const;

    [[nodiscard]] inline T& first();

    [[nodiscard]] inline const T& first() const;

    inline T* pop();

    inline void setSize(IAllocator* p, u32 size);

    inline void setCap(IAllocator* p, u32 cap);

    inline void swapWithLast(u32 i);

    inline void popAsLast(u32 i);

    [[nodiscard]] inline u32 idx(const T* x) const;

    [[nodiscard]] inline u32 lastI() const;

    [[nodiscard]] inline T& at(u32 i);

    [[nodiscard]] inline const T& at(u32 i) const;

    inline void destroy(IAllocator* p);

    [[nodiscard]] inline u32 getSize() const;

    [[nodiscard]] inline u32 getCap() const;

    [[nodiscard]] inline T*& data();

    [[nodiscard]] inline T* const& data() const;

    inline void zeroOut();

    [[nodiscard]] inline VecBase<T> clone(IAllocator* pAlloc) const;

    inline void _grow(IAllocator* p, u32 newCapacity);

    struct It
    {
        T* s;

        It(T* pFirst) : s{pFirst} {}

        T& operator*() { return *s; }
        T* operator->() { return s; }

        It operator++() { ++s; return *this; }
        It operator++(int) { T* tmp = s++; return tmp; }

        It operator--() { --s; return *this; }
        It operator--(int) { T* tmp = s--; return tmp; }

        friend constexpr bool operator==(const It& l, const It& r) { return l.s == r.s; }
        friend constexpr bool operator!=(const It& l, const It& r) { return l.s != r.s; }
    };

    It begin() { return {&this->pData[0]}; }
    It end() { return {&this->pData[this->size]}; }
    It rbegin() { return {&this->pData[this->size - 1]}; }
    It rend() { return {this->pData - 1}; }

    const It begin() const { return {&this->pData[0]}; }
    const It end() const { return {&this->pData[this->size]}; }
    const It rbegin() const { return {&this->pData[this->size - 1]}; }
    const It rend() const { return {this->pData - 1}; }
};

template<typename T>
inline u32
VecBase<T>::push(IAllocator* p, const T& data)
{
    auto* s = this;

    if (s->size >= s->capacity) s->_grow(p, utils::max(s->capacity * 2U, u32(SIZE_MIN)));

    s->pData[s->size++] = data;
    return s->size - 1;
}

template<typename T>
[[nodiscard]] inline T&
VecBase<T>::last()
{
    return this->pData[this->size - 1];
}

template<typename T>
[[nodiscard]] inline const T&
VecBase<T>::last() const
{
    return this->pData[this->size - 1];
}

template<typename T>
[[nodiscard]] inline T&
VecBase<T>::first()
{
    return this->pData[0];
}

template<typename T>
[[nodiscard]] inline const T&
VecBase<T>::first() const
{
    return this->pData[0];
}

template<typename T>
inline T*
VecBase<T>::pop()
{
    assert(this->size > 0 && "[Vec]: pop from empty");
    return &this->pData[--this->size];
}

template<typename T>
inline void
VecBase<T>::setSize(IAllocator* p, u32 size)
{
    if (this->capacity < size) this->_grow(p, size);

    this->size = size;
}

template<typename T>
inline void
VecBase<T>::setCap(IAllocator* p, u32 cap)
{
    auto* s = this;

    s->pData = (T*)p->realloc(s->pData, cap, sizeof(T));
    s->capacity = cap;

    if (s->size > cap) s->size = cap;
}

template<typename T>
inline void
VecBase<T>::swapWithLast(u32 i)
{
    utils::swap(&this->pData[i], &this->pData[this->size - 1]);
}

template<typename T>
inline void
VecBase<T>::popAsLast(u32 i)
{
    this->pData[i] = this->pData[--this->size];
}

template<typename T>
[[nodiscard]] inline u32
VecBase<T>::idx(const T* x) const
{
    u32 r = u32(x - this->pData);
    assert(r < this->capacity);
    return r;
}

template<typename T>
[[nodiscard]] inline u32
VecBase<T>::lastI() const
{
    return this->idx(this->last());
}

template<typename T>
[[nodiscard]] inline T&
VecBase<T>::at(u32 i)
{
    assert(i < this->size && "[Vec]: out of size");
    return this->pData[i];
}

template<typename T>
[[nodiscard]] inline const T&
VecBase<T>::at(u32 i) const
{
    assert(i < this->size && "[Vec]: out of size");
    return this->pData[i];
}

template<typename T>
inline void
VecBase<T>::destroy(IAllocator* p)
{
    p->free(this->pData);
}

template<typename T>
[[nodiscard]] inline u32
VecBase<T>::getSize() const
{
    return this->size;
}

template<typename T>
inline u32
VecBase<T>::getCap() const
{
    return this->capacity;
}

template<typename T>
[[nodiscard]] inline T*&
VecBase<T>::data()
{
    return this->pData;
}

template<typename T>
[[nodiscard]] inline T* const&
VecBase<T>::data() const
{
    return this->pData;
}

template<typename T>
inline void
VecBase<T>::zeroOut()
{
    memset(this->pData, 0, this->size * sizeof(T));
}

template<typename T>
[[nodiscard]] inline VecBase<T>
VecBase<T>::clone(IAllocator* pAlloc) const
{
    auto nVec = VecBase<T>(pAlloc, this->getCap());
    memcpy(nVec.pData, this->data(), this->getSize() * sizeof(T));
    nVec.size = this->getSize();

    return nVec;
}

template<typename T>
inline void
VecBase<T>::_grow(IAllocator* p, u32 newCapacity)
{
    assert(newCapacity * sizeof(T) > 0);
    this->capacity = newCapacity;
    this->pData = (T*)p->realloc(this->pData, newCapacity, sizeof(T));
}

template<typename T>
struct Vec
{
    VecBase<T> base {};
    IAllocator* pAlloc = nullptr;

    Vec() = default;
    Vec(IAllocator* p, u32 prealloc = 1) : base(p, prealloc), pAlloc(p) {}

    T& operator[](u32 i) { return base[i]; }
    const T& operator[](u32 i) const { return base[i]; }

    VecBase<T>::It begin() { return base.begin(); }
    VecBase<T>::It end() { return base.end(); }
    VecBase<T>::It rbegin() { return base.rbegin(); }
    VecBase<T>::It rend() { return rend(); }

    const VecBase<T>::It begin() const { return base.begin(); }
    const VecBase<T>::It end() const { return base.end(); }
    const VecBase<T>::It rbegin() const { return base.rbegin(); }
    const VecBase<T>::It rend() const { return rend(); }

    inline u32 push(const T& data) { return this->base.push(this->pAlloc, data); }

    [[nodiscard]] inline T& VecLast() { return this->base.last(); }

    [[nodiscard]] inline const T& last() const { return this->base.last(); }

    [[nodiscard]] inline T& first() { return this->base.first(); }

    [[nodiscard]] inline const T& first() const { return this->base.first(); }

    inline T* pop() { return this->base.pop(); }

    inline void setSize(u32 size) { this->base.setSize(this->pAlloc, size); }

    inline void setCap(u32 cap) { this->base.setCap(this->pAlloc, cap); }

    inline void swapWithLast(u32 i) { this->base.swapWithLast(i); }

    inline void popAsLast(u32 i) { this->base.popAsLast(i); }

    [[nodiscard]] inline u32 idx(const T* x) const { return this->base.idx(x); }

    [[nodiscard]] inline u32 lastI() const { return this->base.lastI(); }

    [[nodiscard]] inline T& at(u32 i) { return this->base.at(i); }

    [[nodiscard]] inline const T& at(u32 i) const { return this->base.at(i); }

    inline void destroy() { this->base.destroy(this->pAlloc); }

    [[nodiscard]] inline u32 getSize() const { return this->base.getSize(); }

    [[nodiscard]] inline u32 getCap() const { return this->base.getCap(); }

    [[nodiscard]] inline T*& data() { return this->base.data(); }

    [[nodiscard]] inline const T*& data() const { return this->base.data(); }

    inline void zeroOut() { this->base.zeroOut(); }

    [[nodiscard]] inline Vec<T>
    clone(IAllocator* pAlloc)
    {
        auto base = this->base.clone(pAlloc);
        Vec<T> nVec;
        nVec.base = base;
        nVec.pAlloc = pAlloc;
        return nVec;
    }
};

namespace utils
{

template<typename T>
[[nodiscard]] inline bool empty(const VecBase<T>* s) { return s->size == 0; }

template<typename T>
[[nodiscard]] inline bool empty(const Vec<T>* s) { return empty(&s->base); }

} /* namespace utils */

namespace print
{

template<typename T>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const VecBase<T>& x)
{
    if (utils::empty(&x))
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "(empty)");
    }

    char aBuff[1024] {};
    u32 nRead = 0;
    for (u32 i = 0; i < x.size; ++i)
    {
        const char* fmt;
        if constexpr (std::is_floating_point_v<T>) fmt = i == x.size - 1 ? "{:.3}" : "{:.3}, ";
        else fmt = i == x.size - 1 ? "{}" : "{}, ";

        nRead += toBuffer(aBuff + nRead, utils::size(aBuff) - nRead, fmt, x[i]);
    }

    return print::copyBackToBuffer(ctx, aBuff, utils::size(aBuff));
}

template<typename T>
inline u32
formatToContext(Context ctx, FormatArgs fmtArgs, const Vec<T>& x)
{
    return formatToContext(ctx, fmtArgs, x.base);
}

} /* namespace print */

} /* namespace adt */

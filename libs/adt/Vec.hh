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

    [[nodiscard]] bool empty() const { return size == 0; }
    u32 push(IAllocator* p, const T& data);
    [[nodiscard]] T& last();
    [[nodiscard]] const T& last() const;
    [[nodiscard]] T& first();
    [[nodiscard]] const T& first() const;
    T* pop();
    void setSize(IAllocator* p, u32 size);
    void setCap(IAllocator* p, u32 cap);
    void swapWithLast(u32 i);
    void popAsLast(u32 i);
    [[nodiscard]] u32 idx(const T* x) const;
    [[nodiscard]] u32 lastI() const;
    [[nodiscard]] T& at(u32 i);
    [[nodiscard]] const T& at(u32 i) const;
    void destroy(IAllocator* p);
    [[nodiscard]] u32 getSize() const;
    [[nodiscard]] u32 getCap() const;
    [[nodiscard]] T*& data();
    [[nodiscard]] T* const& data() const;
    void zeroOut();
    [[nodiscard]] VecBase<T> clone(IAllocator* pAlloc) const;
    void grow(IAllocator* p, u32 newCapacity);

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
    if (this->size >= this->capacity)
        this->grow(p, utils::max(this->capacity * 2U, u32(SIZE_MIN)));

    this->pData[this->size++] = data;
    return this->size - 1;
}

template<typename T>
[[nodiscard]] inline T&
VecBase<T>::last()
{
    return this->operator[](this->size - 1);
}

template<typename T>
[[nodiscard]] inline const T&
VecBase<T>::last() const
{
    return this->operator[](this->size - 1);
}

template<typename T>
[[nodiscard]] inline T&
VecBase<T>::first()
{
    return this->operator[](0);
}

template<typename T>
[[nodiscard]] inline const T&
VecBase<T>::first() const
{
    return this->operator[](0);
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
    if (this->capacity < size) this->grow(p, size);

    this->size = size;
}

template<typename T>
inline void
VecBase<T>::setCap(IAllocator* p, u32 cap)
{
    this->pData = (T*)p->realloc(this->pData, cap, sizeof(T));
    this->capacity = cap;

    if (this->size > cap) this->size = cap;
}

template<typename T>
inline void
VecBase<T>::swapWithLast(u32 i)
{
    assert(this->size > 0 && "[Vec]: empty");
    utils::swap(&this->pData[i], &this->pData[this->size - 1]);
}

template<typename T>
inline void
VecBase<T>::popAsLast(u32 i)
{
    assert(this->size > 0 && "[Vec]: empty");
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
VecBase<T>::grow(IAllocator* p, u32 newCapacity)
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

    [[nodiscard]] bool empty() const { return base.empty(); }
    u32 push(const T& data) { return this->base.push(this->pAlloc, data); }
    [[nodiscard]] T& VecLast() { return this->base.last(); }
    [[nodiscard]] const T& last() const { return this->base.last(); }
    [[nodiscard]] T& first() { return this->base.first(); }
    [[nodiscard]] const T& first() const { return this->base.first(); }
    T* pop() { return this->base.pop(); }
    void setSize(u32 size) { this->base.setSize(this->pAlloc, size); }
    void setCap(u32 cap) { this->base.setCap(this->pAlloc, cap); }
    void swapWithLast(u32 i) { this->base.swapWithLast(i); }
    void popAsLast(u32 i) { this->base.popAsLast(i); }
    [[nodiscard]] u32 idx(const T* x) const { return this->base.idx(x); }
    [[nodiscard]] u32 lastI() const { return this->base.lastI(); }
    [[nodiscard]] T& at(u32 i) { return this->base.at(i); }
    [[nodiscard]] const T& at(u32 i) const { return this->base.at(i); }
    void destroy() { this->base.destroy(this->pAlloc); }
    [[nodiscard]] u32 getSize() const { return this->base.getSize(); }
    [[nodiscard]] u32 getCap() const { return this->base.getCap(); }
    [[nodiscard]] T*& data() { return this->base.data(); }
    [[nodiscard]] const T*& data() const { return this->base.data(); }
    void zeroOut() { this->base.zeroOut(); }

    [[nodiscard]] Vec<T>
    clone(IAllocator* pAlloc)
    {
        auto base = this->base.clone(pAlloc);
        Vec<T> nVec;
        nVec.base = base;
        nVec.pAlloc = pAlloc;
        return nVec;
    }

    VecBase<T>::It begin() { return base.begin(); }
    VecBase<T>::It end() { return base.end(); }
    VecBase<T>::It rbegin() { return base.rbegin(); }
    VecBase<T>::It rend() { return rend(); }

    const VecBase<T>::It begin() const { return base.begin(); }
    const VecBase<T>::It end() const { return base.end(); }
    const VecBase<T>::It rbegin() const { return base.rbegin(); }
    const VecBase<T>::It rend() const { return rend(); }
};

namespace print
{

template<typename T>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const VecBase<T>& x)
{
    if (x.empty())
    {
        ctx.fmt = "{}";
        ctx.fmtIdx = 0;
        return printArgs(ctx, "(empty)");
    }

    char aBuff[1024] {};
    u32 nRead = 0;
    for (u32 i = 0; i < x.size; ++i)
    {
        const char* fmt = i == x.size - 1 ? "{}" : "{}, ";
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

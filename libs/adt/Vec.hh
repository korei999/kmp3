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
    T* m_pData = nullptr;
    u32 m_size = 0;
    u32 m_capacity = 0;

    /* */

    VecBase() = default;
    VecBase(IAllocator* p, u32 prealloc = 1)
        : m_pData((T*)p->alloc(prealloc, sizeof(T))),
          m_size(0),
          m_capacity(prealloc) {}

    /* */

    T& operator[](u32 i)             { assert(i < m_size && "[Vec] out of size"); return m_pData[i]; }
    const T& operator[](u32 i) const { assert(i < m_size && "[Vec] out of size"); return m_pData[i]; }

    [[nodiscard]] bool empty() const { return m_size == 0; }
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

    /* */

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

    It begin() { return {&this->m_pData[0]}; }
    It end() { return {&this->m_pData[this->m_size]}; }
    It rbegin() { return {&this->m_pData[this->m_size - 1]}; }
    It rend() { return {this->m_pData - 1}; }

    const It begin() const { return {&this->m_pData[0]}; }
    const It end() const { return {&this->m_pData[this->m_size]}; }
    const It rbegin() const { return {&this->m_pData[this->m_size - 1]}; }
    const It rend() const { return {this->m_pData - 1}; }
};

template<typename T>
inline u32
VecBase<T>::push(IAllocator* p, const T& data)
{
    if (this->m_size >= this->m_capacity)
        this->grow(p, utils::max(this->m_capacity * 2U, u32(SIZE_MIN)));

    this->m_pData[this->m_size++] = data;
    return this->m_size - 1;
}

template<typename T>
[[nodiscard]] inline T&
VecBase<T>::last()
{
    return this->operator[](this->m_size - 1);
}

template<typename T>
[[nodiscard]] inline const T&
VecBase<T>::last() const
{
    return this->operator[](this->m_size - 1);
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
    assert(this->m_size > 0 && "[Vec]: pop from empty");
    return &this->m_pData[--this->m_size];
}

template<typename T>
inline void
VecBase<T>::setSize(IAllocator* p, u32 size)
{
    if (this->m_capacity < size) this->grow(p, size);

    this->m_size = size;
}

template<typename T>
inline void
VecBase<T>::setCap(IAllocator* p, u32 cap)
{
    this->m_pData = (T*)p->realloc(this->m_pData, cap, sizeof(T));
    this->m_capacity = cap;

    if (this->m_size > cap) this->m_size = cap;
}

template<typename T>
inline void
VecBase<T>::swapWithLast(u32 i)
{
    assert(this->m_size > 0 && "[Vec]: empty");
    utils::swap(&this->m_pData[i], &this->m_pData[this->m_size - 1]);
}

template<typename T>
inline void
VecBase<T>::popAsLast(u32 i)
{
    assert(this->m_size > 0 && "[Vec]: empty");
    this->m_pData[i] = this->m_pData[--this->m_size];
}

template<typename T>
[[nodiscard]] inline u32
VecBase<T>::idx(const T* x) const
{
    u32 r = u32(x - this->m_pData);
    assert(r < this->m_capacity);
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
    assert(i < this->m_size && "[Vec]: out of size");
    return this->m_pData[i];
}

template<typename T>
[[nodiscard]] inline const T&
VecBase<T>::at(u32 i) const
{
    assert(i < this->m_size && "[Vec]: out of size");
    return this->m_pData[i];
}

template<typename T>
inline void
VecBase<T>::destroy(IAllocator* p)
{
    p->free(this->m_pData);
}

template<typename T>
[[nodiscard]] inline u32
VecBase<T>::getSize() const
{
    return this->m_size;
}

template<typename T>
inline u32
VecBase<T>::getCap() const
{
    return this->m_capacity;
}

template<typename T>
[[nodiscard]] inline T*&
VecBase<T>::data()
{
    return this->m_pData;
}

template<typename T>
[[nodiscard]] inline T* const&
VecBase<T>::data() const
{
    return this->m_pData;
}

template<typename T>
inline void
VecBase<T>::zeroOut()
{
    memset(m_pData, 0, m_size * sizeof(T));
}

template<typename T>
[[nodiscard]] inline VecBase<T>
VecBase<T>::clone(IAllocator* pAlloc) const
{
    auto nVec = VecBase<T>(pAlloc, getCap());
    memcpy(nVec.data(), data(), getSize() * sizeof(T));
    nVec.m_size = getSize();

    return nVec;
}

template<typename T>
inline void
VecBase<T>::grow(IAllocator* p, u32 newCapacity)
{
    assert(newCapacity * sizeof(T) > 0);
    m_capacity = newCapacity;
    m_pData = (T*)p->realloc(m_pData, newCapacity, sizeof(T));
}

template<typename T>
struct Vec
{
    VecBase<T> base {};

    /* */

    IAllocator* m_pAlloc = nullptr;

    /* */

    Vec() = default;
    Vec(IAllocator* p, u32 prealloc = 1) : base(p, prealloc), m_pAlloc(p) {}

    /* */

    T& operator[](u32 i) { return base[i]; }
    const T& operator[](u32 i) const { return base[i]; }

    [[nodiscard]] bool empty() const { return base.empty(); }
    u32 push(const T& data) { return base.push(m_pAlloc, data); }
    [[nodiscard]] T& VecLast() { return base.last(); }
    [[nodiscard]] const T& last() const { return base.last(); }
    [[nodiscard]] T& first() { return base.first(); }
    [[nodiscard]] const T& first() const { return base.first(); }
    T* pop() { return base.pop(); }
    void setSize(u32 size) { base.setSize(m_pAlloc, size); }
    void setCap(u32 cap) { base.setCap(m_pAlloc, cap); }
    void swapWithLast(u32 i) { base.swapWithLast(i); }
    void popAsLast(u32 i) { base.popAsLast(i); }
    [[nodiscard]] u32 idx(const T* x) const { return base.idx(x); }
    [[nodiscard]] u32 lastI() const { return base.lastI(); }
    [[nodiscard]] T& at(u32 i) { return base.at(i); }
    [[nodiscard]] const T& at(u32 i) const { return base.at(i); }
    void destroy() { base.destroy(m_pAlloc); }
    [[nodiscard]] u32 getSize() const { return base.getSize(); }
    [[nodiscard]] u32 getCap() const { return base.getCap(); }
    [[nodiscard]] T*& data() { return base.data(); }
    [[nodiscard]] const T*& data() const { return base.data(); }
    void zeroOut() { base.zeroOut(); }

    [[nodiscard]] Vec<T>
    clone(IAllocator* pAlloc)
    {
        auto nBase = base.clone(pAlloc);
        Vec<T> nVec;
        nVec.base = nBase;
        nVec.m_pAlloc = pAlloc;
        return nVec;
    }

    /* */

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
    for (u32 i = 0; i < x.m_size; ++i)
    {
        const char* fmt = i == x.m_size - 1 ? "{}" : "{}, ";
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

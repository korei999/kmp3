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
template<typename T> struct VecBase;

template<typename T> inline u32
VecPush(VecBase<T>* s, IAllocator* p, const T& data);

template<typename T>
[[nodiscard]] inline T& VecLast(VecBase<T>* s);

template<typename T>
[[nodiscard]] inline const T& VecLast(const VecBase<T>* s);

template<typename T>
[[nodiscard]] inline T& VecFirst(VecBase<T>* s);

template<typename T>
[[nodiscard]] inline const T& VecFirst(const VecBase<T>* s);

template<typename T>
inline T* VecPop(VecBase<T>* s);

template<typename T>
inline void VecSetSize(VecBase<T>* s, IAllocator* p, u32 size);

template<typename T>
inline void VecSetCap(VecBase<T>* s, IAllocator* p, u32 cap);

template<typename T>
inline void VecSwapWithLast(VecBase<T>* s, u32 i);

template<typename T>
inline void VecPopAsLast(VecBase<T>* s, u32 i);

template<typename T>
[[nodiscard]] inline u32 VecIdx(const VecBase<T>* s, const T* x);

template<typename T>
[[nodiscard]] inline u32 VecLastI(const VecBase<T>* s);

template<typename T>
[[nodiscard]] inline T& VecAt(VecBase<T>* s, u32 at);

template<typename T>
[[nodiscard]] inline const T& VecAt(const VecBase<T>* s, u32 at);

template<typename T>
inline void VecDestroy(VecBase<T>* s, IAllocator* p);

template<typename T>
[[nodiscard]] inline u32 VecSize(const VecBase<T>* s);

template<typename T>
inline u32 VecCap(const VecBase<T>* s);

template<typename T>
[[nodiscard]] inline T* VecData(VecBase<T>* s);

template<typename T>
inline void VecZeroOut(VecBase<T>* s);

template<typename T>
[[nodiscard]] inline VecBase<T> VecClone(const VecBase<T>* s, IAllocator* pAlloc);

template<typename T>
inline void _VecGrow(VecBase<T>* s, IAllocator* p, u32 newCapacity);

template<typename T>
struct VecBase
{
    T* pData = nullptr;
    u32 size = 0;
    u32 capacity = 0;

    VecBase() = default;
    VecBase(IAllocator* p, u32 prealloc = 1)
        : pData((T*)alloc(p, prealloc, sizeof(T))),
          size(0),
          capacity(prealloc) {}

    T& operator[](u32 i)             { assert(i < size && "[Vec] out of size"); return pData[i]; }
    const T& operator[](u32 i) const { assert(i < size && "[Vec] out of size"); return pData[i]; }

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
VecPush(VecBase<T>* s, IAllocator* p, const T& data)
{
    if (s->size >= s->capacity) _VecGrow(s, p, utils::max(s->capacity * 2U, u32(SIZE_MIN)));

    s->pData[s->size++] = data;
    return s->size - 1;
}

template<typename T>
[[nodiscard]] inline T&
VecLast(VecBase<T>* s)
{
    return s->pData[s->size - 1];
}

template<typename T>
[[nodiscard]] inline const T&
VecLast(const VecBase<T>* s)
{
    return s->pData[s->size - 1];
}

template<typename T>
[[nodiscard]] inline T&
VecFirst(VecBase<T>* s)
{
    return s->pData[0];
}

template<typename T>
[[nodiscard]] inline const T&
VecFirst(const VecBase<T>* s)
{
    return s->pData[0];
}

template<typename T>
inline T*
VecPop(VecBase<T>* s)
{
    assert(s->size > 0 && "[Vec]: pop from empty");
    return &s->pData[--s->size];
}

template<typename T>
inline void
VecSetSize(VecBase<T>* s, IAllocator* p, u32 size)
{
    if (s->capacity < size) _VecGrow(s, p, size);

    s->size = size;
}

template<typename T>
inline void
VecSetCap(VecBase<T>* s, IAllocator* p, u32 cap)
{
    s->pData = (T*)realloc(p, s->pData, cap, sizeof(T));
    s->capacity = cap;

    if (s->size > cap) s->size = cap;
}

template<typename T>
inline void
VecSwapWithLast(VecBase<T>* s, u32 i)
{
    utils::swap(&s->pData[i], &s->pData[s->size - 1]);
}

template<typename T>
inline void
VecPopAsLast(VecBase<T>* s, u32 i)
{
    s->pData[i] = s->pData[--s->size];
}

template<typename T>
[[nodiscard]] inline u32
VecIdx(const VecBase<T>* s, const T* x)
{
    u32 r = u32(x - s->pData);
    assert(r < s->capacity);
    return r;
}

template<typename T>
[[nodiscard]] inline u32
VecLastI(const VecBase<T>* s)
{
    return VecIdx(s, &VecLast(s));
}

template<typename T>
[[nodiscard]] inline T&
VecAt(VecBase<T>* s, u32 at)
{
    assert(at < s->size && "[Vec]: out of size");
    return s->pData[at];
}

template<typename T>
[[nodiscard]] inline const T&
VecAt(const VecBase<T>* s, u32 at)
{
    assert(at < s->size && "[Vec]: out of size");
    return s->pData[at];
}

template<typename T>
inline void
VecDestroy(VecBase<T>* s, IAllocator* p)
{
    free(p, s->pData);
}

template<typename T>
[[nodiscard]] inline u32
VecSize(const VecBase<T>* s)
{
    return s->size;
}

template<typename T>
inline u32
VecCap(const VecBase<T>* s)
{
    return s->capacity;
}

template<typename T>
[[nodiscard]] inline T*
VecData(VecBase<T>* s)
{
    return s->pData;
}

template<typename T>
inline void
VecZeroOut(VecBase<T>* s)
{
    memset(s->pData, 0, s->size * sizeof(T));
}

template<typename T>
[[nodiscard]] inline VecBase<T>
VecClone(const VecBase<T>* s, IAllocator* pAlloc)
{
    auto nVec = VecBase<T>(pAlloc, s->capacity);
    memcpy(nVec.pData, s->pData, s->size * sizeof(T));
    nVec.size = s->size;

    return nVec;
}

template<typename T>
inline void
_VecGrow(VecBase<T>* s, IAllocator* p, u32 newCapacity)
{
    assert(newCapacity * sizeof(T) > 0);
    s->capacity = newCapacity;
    s->pData = (T*)realloc(p, s->pData, newCapacity, sizeof(T));
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
};

template<typename T>
inline u32 VecPush(Vec<T>* s, const T& data) { return VecPush<T>(&s->base, s->pAlloc, data); }

template<typename T>
[[nodiscard]] inline T& VecLast(Vec<T>* s) { return VecLast<T>(&s->base); }

template<typename T>
[[nodiscard]] inline const T& VecLast(Vec<T>* s) { return VecLast<T>(&s->base); }

template<typename T>
[[nodiscard]] inline T& VecFirst(Vec<T>* s) { return VecFirst<T>(&s->base); }

template<typename T>
[[nodiscard]] inline const T& VecFirst(const Vec<T>* s) { return VecFirst<T>(&s->base); }

template<typename T>
inline T* VecPop(Vec<T>* s) { return VecPop<T>(&s->base); }

template<typename T>
inline void VecSetSize(Vec<T>* s, u32 size) { VecSetSize<T>(&s->base, s->pAlloc, size); }

template<typename T>
inline void VecSetCap(Vec<T>* s, u32 cap) { VecSetCap<T>(&s->base, s->pAlloc, cap); }

template<typename T>
inline void VecSwapWithLast(Vec<T>* s, u32 i) { VecSwapWithLast<T>(&s->base, i); }

template<typename T>
inline void VecPopAsLast(Vec<T>* s, u32 i) { VecPopAsLast<T>(&s->base, i); }

template<typename T>
[[nodiscard]] inline u32 VecIdx(const Vec<T>* s, const T* x) { return VecIdx<T>(&s->base, x); }

template<typename T>
[[nodiscard]] inline u32 VecLastI(const Vec<T>* s) { return VecLastI<T>(&s->base); }

template<typename T>
[[nodiscard]] inline T& VecAt(Vec<T>* s, u32 at) { return VecAt<T>(&s->base, at); }

template<typename T>
[[nodiscard]] inline const T& VecAt(const Vec<T>* s, u32 at) { return VecAt<T>(&s->base, at); }

template<typename T>
inline void VecDestroy(Vec<T>* s) { VecDestroy<T>(&s->base, s->pAlloc); }

template<typename T>
inline u32 VecSize(const Vec<T>* s) { return VecSize<T>(&s->base); }

template<typename T>
inline u32 VecCap(const Vec<T>* s) { return VecCap<T>(&s->base); }

template<typename T>
[[nodiscard]] inline T* VecData(Vec<T>* s) { return VecData<T>(&s->base); }

template<typename T>
inline void VecZeroOut(Vec<T>* s) { VecZeroOut<T>(&s->base); }

template<typename T>
[[nodiscard]] inline Vec<T>
VecClone(const Vec<T>* s, IAllocator* pAlloc)
{
    auto base = VecClone(&s->base, pAlloc);
    Vec<T> nVec;
    nVec.base = base;
    nVec.pAlloc = pAlloc;
    return nVec;
}

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

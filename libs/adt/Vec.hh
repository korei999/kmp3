#pragma once

#include "Allocator.hh"
#include "utils.hh"
#include "print.hh"

#include "logs.hh"

#include <cassert>

namespace adt
{

#define ADT_VEC_FOREACH_I(A, I) for (u32 I = 0; I < (A)->size; I++)
#define ADT_VEC_FOREACH_I_REV(A, I) for (u32 I = (A)->size - 1; I != -1U ; I--)

/* Dynamic array (aka Vector), use outside Allocator for each allocating operation explicitly */
template<typename T>
struct VecBase
{
    T* pData = nullptr;
    u32 size = 0;
    u32 capacity = 0;

    VecBase() = default;
    VecBase(Allocator* p, u32 prealloc = 1)
        : pData {(T*)alloc(p, prealloc, sizeof(T))},
          size {0},
          capacity {prealloc} {}

    T& operator[](u32 i)             { assert(i < capacity && "out of range vec access"); return pData[i]; }
    const T& operator[](u32 i) const { assert(i < capacity && "out of range vec access"); return pData[i]; }

    struct It
    {
        T* s;

        It(T* pFirst) : s{pFirst} {}

        T& operator*() { return *s; }
        T* operator->() { return s; }

        It operator++() { s++; return *this; }
        It operator++(int) { T* tmp = s++; return tmp; }

        It operator--() { s--; return *this; }
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
inline void
VecGrow(VecBase<T>* s, Allocator* p, u32 size)
{
    /*T* pNew = (T*)alloc(p, size, sizeof(T));*/
    /*memcpy(pNew, s->pData, s->size * sizeof(T));*/
    /*free(p, s->pData);*/
    /*s->cap = size;*/
    /*s->pData = pNew;*/

    assert(s->size * sizeof(T) > 0);

    /*LOG_NOTIFY(*/
    /*    "VecGrow:                                  to copy: {}, newSize: {}, [{}, {}]\n",*/
    /*    s->size * sizeof(T), size*sizeof(T), (u8*)s->pData, (u8*)s->pData + s->size*sizeof(T)*/
    /*);*/

    s->capacity = size;
    s->pData = (T*)realloc(p, s->pData, size, sizeof(T));
}

template<typename T>
inline u32
VecPush(VecBase<T>* s, Allocator* p, const T& data)
{
    if (s->capacity == 0) *s = {p, SIZE_MIN};

    assert(s->capacity > 0 && "VecBase: uninitialized push");

    if (s->size >= s->capacity) VecGrow(s, p, s->capacity * 2);

    s->pData[s->size++] = data;
    return s->size - 1;
}

template<typename T>
inline T&
VecLast(VecBase<T>* s)
{
    return s->pData[s->size - 1];
}

template<typename T>
inline const T&
VecLast(const VecBase<T>* s)
{
    return s->pData[s->size - 1];
}

template<typename T>
inline T&
VecFirst(VecBase<T>* s)
{
    return s->pData[0];
}

template<typename T>
inline const T&
VecFirst(const VecBase<T>* s)
{
    return s->pData[0];
}

template<typename T>
inline T*
VecPop(VecBase<T>* s)
{
    assert(s->size > 0 && "VecBase: empty pop");
    return &s->pData[--s->size];
}

template<typename T>
inline void
VecSetSize(VecBase<T>* s, Allocator* p, u32 size)
{
    if (s->capacity < size) VecGrow(s, p, size);

    s->size = size;
}

template<typename T>
inline void
VecSetCap(VecBase<T>* s, Allocator* p, u32 cap)
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
inline u32
VecIdx(const VecBase<T>* s, const T* x)
{
    u32 r = u32(x - s->pData);
    assert(r < s->capacity);
    return r;
}

template<typename T>
inline u32
VecLastI(const VecBase<T>* s)
{
    return VecIdx(s, &VecLast(s));
}

template<typename T>
inline T&
VecAt(VecBase<T>* s, u32 at)
{
    assert(at < s->size && "VecBase: out of size range");
    return s->pData[at];
}

template<typename T>
inline const T&
VecAt(const VecBase<T>* s, u32 at)
{
    assert(at < s->size && "VecBase: out of size range");
    return s->pData[at];
}

template<typename T>
inline void
VecDestroy(VecBase<T>* s, Allocator* p)
{
    free(p, s->pData);
}

template<typename T>
inline u32
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
inline T*
VecData(VecBase<T>* s)
{
    return s->pData;
}

/* Dynamic array (aka Vector), with Allocator* stored */
template<typename T>
struct Vec
{
    VecBase<T> base {};
    Allocator* pAlloc = nullptr;

    Vec() = default;
    Vec(Allocator* p, u32 _cap = 1)
        : base(p, _cap), pAlloc(p) {}

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
inline void
VecGrow(Vec<T>* s, u32 size)
{
    VecGrow(&s->base, s->pAlloc, size);
}

template<typename T>
inline u32
VecPush(Vec<T>* s, const T& data)
{
    return VecPush(&s->base, s->pAlloc, data);
}

template<typename T>
inline T&
VecLast(Vec<T>* s)
{
    return VecLast(&s->base);
}

template<typename T>
inline const T&
VecLast(Vec<T>* s)
{
    return VecLast(&s->base);
}

template<typename T>
inline T&
VecFirst(Vec<T>* s)
{
    return VecFirst(&s->base);
}

template<typename T>
inline const T&
VecFirst(const Vec<T>* s)
{
    return VecFirst(&s->base);
}

template<typename T>
inline T*
VecPop(Vec<T>* s)
{
    return VecPop(&s->base);
}

template<typename T>
inline void
VecSetSize(Vec<T>* s, u32 size)
{
    VecSetSize(&s->base, s->pAlloc, size);
}

template<typename T>
inline void
VecSetCap(Vec<T>* s, u32 cap)
{
    VecSetCap(&s->base, s->pAlloc, cap);
}

template<typename T>
inline void
VecSwapWithLast(Vec<T>* s, u32 i)
{
    VecSwapWithLast(&s->base, i);
}

template<typename T>
inline void
VecPopAsLast(Vec<T>* s, u32 i)
{
    VecPopAsLast(&s->base, i);
}

template<typename T>
inline u32
VecIdx(const Vec<T>* s, const T* x)
{
    return VecIdx(&s->base, x);
}

template<typename T>
inline u32
VecLastI(const Vec<T>* s)
{
    return VecLastI(&s->base);
}

template<typename T>
inline T&
VecAt(Vec<T>* s, u32 at)
{
    return VecAt(&s->base, at);
}

template<typename T>
inline const T&
VecAt(const Vec<T>* s, u32 at)
{
    return VecAt(&s->base, at);
}

template<typename T>
inline void
VecDestroy(Vec<T>* s)
{
    VecDestroy(&s->base, s->pAlloc);
}

template<typename T>
inline u32
VecSize(const Vec<T>* s)
{
    return VecSize(&s->base);
}

template<typename T>
inline u32
VecCap(const Vec<T>* s)
{
    return VecCap(&s->base);
}

template<typename T>
inline T*
VecData(Vec<T>* s)
{
    return VecData(&s->base);
}

namespace print
{

template<typename T>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const VecBase<T>& x)
{
    if (x.size == 0)
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

} /* namespace print */

} /* namespace adt */

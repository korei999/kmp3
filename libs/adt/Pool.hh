#pragma once

#include "types.hh"
#include "guard.hh"
#include "Arr.hh"

#include <cstdio>
#include <cassert>
#include <limits>

#include <threads.h>

namespace adt
{

using PoolHnd = u32;

template<typename T>
struct PoolNode
{
    T data {};
    bool bDeleted {};
};

template<typename T, u32 CAP> struct Pool;

template<typename T, u32 CAP>
inline s64 PoolFirstIdx(Pool<T, CAP>* s);

template<typename T, u32 CAP>
inline s64 PoolLastIdx(Pool<T, CAP>* s);

template<typename T, u32 CAP>
inline s64 PoolNextIdx(Pool<T, CAP>* s, s64 i);

template<typename T, u32 CAP>
inline s64 PoolPrevIdx(Pool<T, CAP>* s, s64 i);

template<typename T, u32 CAP>
inline u32 PoolIdx(const Pool<T, CAP>* s, const PoolNode<T>* p);

template<typename T, u32 CAP>
inline u32 PoolIdx(const Pool<T, CAP>* s, const T* p);

template<typename T, u32 CAP>
inline void PoolDestroy(Pool<T, CAP>* s);

template<typename T, u32 CAP>
[[nodiscard]] inline PoolHnd PoolRent(Pool<T, CAP>* s);

template<typename T, u32 CAP>
inline PoolHnd PoolRent(Pool<T, CAP>* s, const T& value);

template<typename T, u32 CAP>
inline void PoolReturn(Pool<T, CAP>* s, PoolHnd hnd);

/* statically allocated reusable resource collection */
template<typename T, u32 CAP>
struct Pool
{
    Arr<PoolNode<T>, CAP> aNodes {};
    Arr<PoolHnd, CAP> aFreeIdxs {};
    u32 nOccupied {};
    mtx_t mtx;

    T& operator[](s64 i) { assert(!aNodes[i].bDeleted && "[MemPool]: accessing deleted node"); return aNodes[i].data; }
    const T& operator[](s64 i) const { assert(!aNodes[i].bDeleted && "[MemPool]: accessing deleted node"); return aNodes[i].data; }

    Pool() = default;
    Pool(INIT_FLAG e)
    { 
        if (e != INIT_FLAG::INIT) return;

        mtx_init(&mtx, mtx_plain);
        for (auto& e : this->aNodes) e.bDeleted = true;
    }

    struct It
    {
        Pool* s {};
        s64 i {};

        It(Pool* _self, s64 _i) : s(_self), i(_i) {}

        T& operator*() { return s->aNodes[i].data; }
        T* operator->() { return &s->aNodes[i].data; }

        It
        operator++()
        {
            i = PoolNextIdx(s, i);
            return {s, i};
        }

        It
        operator++(int)
        {
            s64 tmp = i;
            i = PoolNextIdx(s, i);
            return {s, tmp};
        }

        It
        operator--()
        {
            i = PoolPrevIdx(s, i);
            return {s, i};
        }

        It
        operator--(int)
        {
            s64 tmp = i;
            i = PoolPrevIdx(s, i);
            return {s, tmp};
        }

        friend bool operator==(It l, It r) { return l.i == r.i; }
        friend bool operator!=(It l, It r) { return l.i != r.i; }
    };

    It begin() { return {this, PoolFirstIdx(this)}; }
    It end() { return {this, this->aNodes.size == 0 ? -1 : PoolLastIdx(this) + 1}; }
    It rbegin() { return {this, PoolLastIdx(this)}; }
    It rend() { return {this, this->aNodes.size == 0 ? -1 : PoolFirstIdx(this) - 1}; }

    const It begin() const { return {this, PoolFirstIdx(this)}; }
    const It end() const { return {this, this->aNodes.size == 0 ? -1 : PoolLastIdx(this) + 1}; }
    const It rbegin() const { return {this, PoolLastIdx(this)}; }
    const It rend() const { return {this, this->aNodes.size == 0 ? -1 : PoolFirstIdx(this) - 1}; }
};

template<typename T, u32 CAP>
inline s64
PoolFirstIdx(Pool<T, CAP>* s)
{
    if (s->aNodes.size == 0) return -1;

    for (u32 i = 0; i < s->aNodes.size; ++i)
        if (!s->aNodes[i].bDeleted) return i;

    return s->aNodes.size;
}

template<typename T, u32 CAP>
inline s64
PoolLastIdx(Pool<T, CAP>* s)
{
    if (s->aNodes.size == 0) return -1;

    for (s64 i = s64(s->aNodes.size) - 1; i >= 0; --i)
        if (!s->aNodes[i].bDeleted) return i;

    return s->aNodes.size;
}

template<typename T, u32 CAP>
inline s64
PoolNextIdx(Pool<T, CAP>* s, s64 i)
{
    do ++i;
    while (i < s->aNodes.size && s->aNodes[i].bDeleted);

    return i;
}

template<typename T, u32 CAP>
inline s64
PoolPrevIdx(Pool<T, CAP>* s, s64 i)
{
    do --i;
    while (i >= 0 && s->aNodes[i].bDeleted);

    return i;
}

template<typename T, u32 CAP>
inline u32
PoolIdx(const Pool<T, CAP>* s, const PoolNode<T>* p)
{
    u32 r = p - &s->aNodes.aData[0];
    assert(r < CAP && "[Pool]: out of range");
    return r;
}

template<typename T, u32 CAP>
inline u32
PoolIdx(const Pool<T, CAP>* s, const T* p)
{
    return (PoolNode<T>*)p - &s->aNodes.aData[0];
}

template<typename T, u32 CAP>
inline void
PoolDestroy(Pool<T, CAP>* s)
{
    mtx_destroy(&s->mtx);
}

template<typename T, u32 CAP>
[[nodiscard]] inline PoolHnd
PoolRent(Pool<T, CAP>* s)
{
    guard::Mtx lock(&s->mtx);

    PoolHnd ret = std::numeric_limits<PoolHnd>::max();

    if (s->nOccupied >= CAP)
    {
#ifndef NDEBUG
        fputs("[MemPool]: no free element, returning NPOS", stderr);
#endif
        return ret;
    }

    ++s->nOccupied;

    if (s->aFreeIdxs.size > 0) ret = *ArrPop(&s->aFreeIdxs);
    else ret = ArrFakePush(&s->aNodes);

    s->aNodes[ret].bDeleted = false;
    return ret;
}

template<typename T, u32 CAP>
inline PoolHnd
PoolRent(Pool<T, CAP>* s, const T& value)
{
    auto idx = PoolRent(s);
    (*s)[idx] = value;

    return idx;
}

template<typename T, u32 CAP>
inline void
PoolReturn(Pool<T, CAP>* s, PoolHnd hnd)
{
    guard::Mtx lock(&s->mtx);

    --s->nOccupied;
    assert(s->nOccupied < CAP && "[Pool]: nothing to return");

    if (hnd == ArrSize(&s->aNodes) - 1) ArrFakePop(&s->aNodes);
    else
    {
        ArrPush(&s->aFreeIdxs, hnd);
        auto& node = s->aNodes[hnd];
        assert(!node.bDeleted && "[Pool]: returning already deleted node");
        node.bDeleted = true;
    }
}

} /* namespace adt */

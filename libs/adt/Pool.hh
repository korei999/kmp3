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

    s64 firstI() const;

    s64 lastI() const;

    s64 nextI(s64 i) const;

    s64 prevI(s64 i) const;

    u32 idx(const PoolNode<T>* p) const;

    u32 idx(const T* p) const;

    void destroy();

    [[nodiscard]] PoolHnd getHandle();

    [[nodiscard]] PoolHnd getHandle(const T& value);

    void giveBack(PoolHnd hnd);

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
            i = s->nextI(i);
            return {s, i};
        }

        It
        operator++(int)
        {
            s64 tmp = i;
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
            s64 tmp = i;
            i = s->prevI(i);
            return {s, tmp};
        }

        friend bool operator==(It l, It r) { return l.i == r.i; }
        friend bool operator!=(It l, It r) { return l.i != r.i; }
    };

    It begin() { return {this, this->firstI()}; }
    It end() { return {this, this->aNodes.size == 0 ? -1 : this->lastI() + 1}; }
    It rbegin() { return {this, PoolLastIdx(this)}; }
    It rend() { return {this, this->aNodes.size == 0 ? -1 : this->firstI() - 1}; }

    const It begin() const { return {this, this->firstI()}; }
    const It end() const { return {this, this->aNodes.size == 0 ? -1 : this->lastI() + 1}; }
    const It rbegin() const { return {this, PoolLastIdx(this)}; }
    const It rend() const { return {this, this->aNodes.size == 0 ? -1 : this->firstI() - 1}; }
};

template<typename T, u32 CAP>
inline s64
Pool<T, CAP>::firstI() const
{
    if (this->aNodes.size == 0) return -1;

    for (u32 i = 0; i < this->aNodes.size; ++i)
        if (!this->aNodes[i].bDeleted) return i;

    return this->aNodes.size;
}

template<typename T, u32 CAP>
inline s64
Pool<T, CAP>::lastI() const
{
    if (this->aNodes.size == 0) return -1;

    for (s64 i = s64(this->aNodes.size) - 1; i >= 0; --i)
        if (!this->aNodes[i].bDeleted) return i;

    return this->aNodes.size;
}

template<typename T, u32 CAP>
inline s64
Pool<T, CAP>::nextI(s64 i) const
{
    do ++i;
    while (i < this->aNodes.size && this->aNodes[i].bDeleted);

    return i;
}

template<typename T, u32 CAP>
inline s64
Pool<T, CAP>::prevI(s64 i) const
{
    do --i;
    while (i >= 0 && this->aNodes[i].bDeleted);

    return i;
}

template<typename T, u32 CAP>
inline u32
Pool<T, CAP>::idx(const PoolNode<T>* p) const
{
    u32 r = p - &this->aNodes.aData[0];
    assert(r < CAP && "[Pool]: out of range");
    return r;
}

template<typename T, u32 CAP>
inline u32
Pool<T, CAP>::idx(const T* p) const
{
    return (PoolNode<T>*)p - &this->aNodes.aData[0];
}

template<typename T, u32 CAP>
inline void
Pool<T, CAP>::destroy()
{
    mtx_destroy(&this->mtx);
}

template<typename T, u32 CAP>
inline PoolHnd
Pool<T, CAP>::getHandle()
{
    guard::Mtx lock(&this->mtx);

    PoolHnd ret = std::numeric_limits<PoolHnd>::max();

    if (this->nOccupied >= CAP)
    {
#ifndef NDEBUG
        fputs("[MemPool]: no free element, returning NPOS", stderr);
#endif
        return ret;
    }

    ++this->nOccupied;

    if (this->aFreeIdxs.size > 0) ret = *this->aFreeIdxs.pop();
    else ret = this->aNodes.fakePush();

    this->aNodes[ret].bDeleted = false;
    return ret;
}

template<typename T, u32 CAP>
inline PoolHnd
Pool<T, CAP>::getHandle(const T& value)
{
    auto idx = this->getHandle();
    (*this)[idx] = value;

    return idx;
}

template<typename T, u32 CAP>
inline void
Pool<T, CAP>::giveBack(PoolHnd hnd)
{
    guard::Mtx lock(&this->mtx);

    --this->nOccupied;
    assert(this->nOccupied < CAP && "[Pool]: nothing to return");

    if (hnd == this->aNodes.getSize() - 1) this->aNodes.fakePop();
    else
    {
        this->aFreeIdxs.push(hnd);
        auto& node = this->aNodes[hnd];
        assert(!node.bDeleted && "[Pool]: returning already deleted node");
        node.bDeleted = true;
    }
}

} /* namespace adt */

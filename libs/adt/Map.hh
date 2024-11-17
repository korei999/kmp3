#pragma once

#include "Vec.hh"
#include "hash.hh"

namespace adt
{

constexpr f32 MAP_DEFAULT_LOAD_FACTOR = 0.5f;
constexpr f32 MAP_DEFAULT_LOAD_FACTOR_INV = 1.0f / MAP_DEFAULT_LOAD_FACTOR;

template<typename T> struct MapBase;

template<typename T> inline void _MapRehash(MapBase<T>* s, Allocator* p, u32 size);

template<typename T>
struct Bucket
{
    T data;
    bool bOccupied = false;
    bool bDeleted = false;
};

/* custom return type for insert/search operations */
template<typename T>
struct MapResult
{
    T* pData;
    u64 hash;
    bool bInserted;

    constexpr explicit operator bool() const
    {
        return this->pData != nullptr;
    }
};

/* Value is the key in this map.
 * For key/value pairs, use struct { KEY k; VALUE v; }, add `u64 adt::hash::func(const struct&)`
 * and* bool operator==(const struct& l, const struct& r). */
template<typename T>
struct MapBase
{
    VecBase<Bucket<T>> aBuckets;
    f32 maxLoadFactor;
    u32 bucketCount = 0;

    MapBase() = default;
    MapBase(Allocator* pAllocator, u32 prealloc = SIZE_MIN);

    struct It
    {
        MapBase* s {};
        u32 i = 0;

        It(MapBase* _s, u32 _i) : s {_s}, i {_i} {}

        T& operator*() { return s->aBuckets[i].data; }
        T* operator->() { return &s->aBuckets[i].data; }

        It operator++()
        {
            i = MapNextI(s, i);
            return {s, i};
        }
        It operator++(int) { T* tmp = s++; return tmp; }

        friend constexpr bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend constexpr bool operator!=(const It& l, const It& r) { return l.i != r.i; }
    };

    It begin() { return {this, MapFirstI(this)}; }
    It end() { return {this, NPOS}; }

    const It begin() const { return {this, MapFirstI(this)}; }
    const It end() const { return {this, NPOS}; }
};

template<typename T>
inline u32
MapIdx(MapBase<T>* s, T* p)
{
    return p - VecData(&s->aBuckets);
}

template<typename T>
inline u32
MapIdx(MapBase<T>* s, MapResult<T>* pRes)
{
    return pRes->pData - VecData(&s->aBuckets);
}

template<typename T>
inline u32
MapFirstI(MapBase<T>* s)
{
    u32 i = 0;
    while (i < VecCap(&s->aBuckets) && !s->aBuckets[i].bOccupied)
        i++;

    if (i >= VecCap(&s->aBuckets)) i = NPOS;

    return i;
}

template<typename T>
inline u32
MapNextI(MapBase<T>* s, u32 i)
{
    ++i;
    while (i < VecCap(&s->aBuckets) && !s->aBuckets[i].bOccupied)
        i++;

    if (i >= VecCap(&s->aBuckets)) i = NPOS;

    return i;
}

template<typename T>
inline f32
MapLoadFactor(MapBase<T>* s)
{
    return f32(s->bucketCount) / f32(VecCap(&s->aBuckets));
}

template<typename T>
inline MapResult<T>
MapInsert(MapBase<T>* s, Allocator* p, const T& value)
{
    if (VecCap(&s->aBuckets) == 0) *s = {p};

    if (MapLoadFactor(s) >= s->maxLoadFactor)
        _MapRehash(s, p, VecCap(&s->aBuckets) * 2);

    u64 hash = hash::func(value);
    u32 idx = u32(hash % VecCap(&s->aBuckets));

    while (s->aBuckets[idx].bOccupied)
    {
        idx++;
        if (idx >= VecCap(&s->aBuckets))
            idx = 0;
    }

    s->aBuckets[idx].data = value;
    s->aBuckets[idx].bOccupied = true;
    s->aBuckets[idx].bDeleted = false;
    s->bucketCount++;

    return {
        .pData = &s->aBuckets[idx].data,
        .hash = hash,
        .bInserted = true
    };
}

template<typename T>
inline MapResult<T>
MapSearch(MapBase<T>* s, const T& value)
{
    assert(s && "MapBase: map is nullptr");

    if (s->bucketCount == 0) return {};

    u64 hash = hash::func(value);
    u32 idx = u32(hash % VecCap(&s->aBuckets));

    MapResult<T> ret;
    ret.hash = hash;
    ret.pData = nullptr;
    ret.bInserted = false;

    while (s->aBuckets[idx].bOccupied || s->aBuckets[idx].bDeleted)
    {
        if (s->aBuckets[idx].data == value)
        {
            ret.pData = &s->aBuckets[idx].data;
            break;
        }

        idx++;
        if (idx >= VecCap(&s->aBuckets)) idx = 0;
    }

    /*ret.idx = idx;*/
    return ret;
}

template<typename T>
inline void
MapRemove(MapBase<T>*s, u32 i)
{
    s->aBuckets[i].bDeleted = true;
    s->aBuckets[i].bOccupied = false;

    s->bucketCount--;
}

template<typename T>
inline void
MapRemove(MapBase<T>*s, const T& x)
{
    auto f = MapSearch(s, x);
    MapRemove(s, f.idx);
}

template<typename T>
inline void
_MapRehash(MapBase<T>* s, Allocator* p, u32 size)
{
    auto mNew = MapBase<T>(p, size);

    for (u32 i = 0; i < VecCap(&s->aBuckets); i++)
        if (s->aBuckets[i].bOccupied)
            MapInsert(&mNew, p, s->aBuckets[i].data);

    MapDestroy(s, p);
    *s = mNew;
}

template<typename T>
inline MapResult<T>
MapTryInsert(MapBase<T>* s, Allocator* p, const T& value)
{
    auto f = MapSearch(s, value);
    if (f) return f;
    else return MapInsert(s, p, value);
}

template<typename T>
inline void
MapDestroy(MapBase<T>* s, Allocator* p)
{
    VecDestroy(&s->aBuckets, p);
}

template<typename T>
MapBase<T>::MapBase(Allocator* pAllocator, u32 prealloc)
    : aBuckets(pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV),
      maxLoadFactor(MAP_DEFAULT_LOAD_FACTOR)
{
    VecSetSize(&aBuckets, pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV);
}

template<typename T>
struct Map
{
    MapBase<T> base {};
    Allocator* pA {};

    Map() = default;
    Map(Allocator* _pA, u32 prealloc = SIZE_MIN)
        : base(_pA, prealloc), pA(_pA) {}

    MapBase<T>::It begin() { return base.begin(); }
    MapBase<T>::It end() { return base.end(); }
    MapBase<T>::It rbegin() { return base.rbegin(); }
    MapBase<T>::It rend() { return rend(); }

    const MapBase<T>::It begin() const { return base.begin(); }
    const MapBase<T>::It end() const { return base.end(); }
    const MapBase<T>::It rbegin() const { return base.rbegin(); }
    const MapBase<T>::It rend() const { return rend(); }
};

template<typename T> inline u32 MapIdx(Map<T>* s, T* p) { return MapIdx(&s->base, p); }
template<typename T> inline u32 MapIdx(Map<T>* s, MapResult<T>* pRes) { return MapIdx(s->base, pRes); }
template<typename T> inline u32 MapFirstI(Map<T>* s) { return MapFirstI(&s->base); }
template<typename T> inline u32 MapNextI(Map<T>* s, u32 i) { return MapNextI(&s->base, i); }
template<typename T> inline f32 MapLoadFactor(Map<T>* s) { return MapLoadFactor(&s->base); }
template<typename T> inline MapResult<T> MapInsert(Map<T>* s, const T& value) { return MapInsert(&s->base, s->pA, value); }
template<typename T> inline MapResult<T> MapSearch(Map<T>* s, const T& value) { return MapSearch(&s->base, value); }
template<typename T> inline void MapRemove(Map<T>*s, u32 i) { MapRemove(&s->base, i); }
template<typename T> inline void MapRemove(Map<T>*s, const T& x) { MapRemove(&s->base, x); }
template<typename T> inline void _MapRehash(Map<T>* s, u32 size) { _MapRehash(&s->base, s->pA, size); }
template<typename T> inline MapResult<T> MapTryInsert(Map<T>* s, const T& value) { return MapTryInsert(&s->base, s->pA, value); }
template<typename T> inline void MapDestroy(Map<T>* s) { MapDestroy(&s->base, s->pA); }

} /* namespace adt */

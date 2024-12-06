/* simple hashmap with linear probing.
 * For customr key types, add template<> hash::func(const key& x), (or template<> hash::funcHVal(const key& x, u64 hval) for reusable hash)
 * and bool operator==(const key& other) */

#pragma once

#include "Vec.hh"
#include "hash.hh"

namespace adt
{

constexpr f32 MAP_DEFAULT_LOAD_FACTOR = 0.5f;
constexpr f32 MAP_DEFAULT_LOAD_FACTOR_INV = 1.0f / MAP_DEFAULT_LOAD_FACTOR;

enum class MAP_RESULT_STATUS : u8 { FOUND, NOT_FOUND, INSERTED };

template<typename K, typename V>
struct KeyVal
{
    K key {};
    V val {};
};

template<typename K, typename V>
struct MapBucket
{
    K key {};
    V val {};
    bool bOccupied = false;
    bool bDeleted = false;
    /* keep this order for iterators */
};

/* custom return type for insert/search operations */
template<typename K, typename V>
struct MapResult
{
    MapBucket<K, V>* pData {};
    u64 hash {};
    MAP_RESULT_STATUS eStatus {};

    constexpr explicit operator bool() const
    {
        return this->pData != nullptr;
    }
};

template<typename K, typename V> struct MapBase;

template<typename K, typename V>
inline u32 MapIdx(MapBase<K, V>* s, KeyVal<K, V>* p);

template<typename K, typename V>
inline u32 MapIdx(MapBase<K, V>* s, MapResult<K, V> res);

template<typename K, typename V>
inline u32 MapFirstI(MapBase<K, V>* s);

template<typename K, typename V>
inline u32 MapNextI(MapBase<K, V>* s, u32 i);

template<typename K, typename V>
inline f32 MapLoadFactor(MapBase<K, V>* s);

template<typename K, typename V>
inline MapResult<K, V> MapInsert(MapBase<K ,V>* s, IAllocator* p, const K& key, const V& val);

template<typename K, typename V>
[[nodiscard]] inline MapResult<K, V> MapSearch(MapBase<K, V>* s, const K& key);

template<typename K, typename V>
inline void MapRemove(MapBase<K, V>*s, u32 i);

template<typename K, typename V>
inline void MapRemove(MapBase<K, V>*s, const K& key);

template<typename K, typename V>
inline MapResult<K, V> MapTryInsert(MapBase<K, V>* s, IAllocator* p, const K& key, const V& val);

template<typename K, typename V>
inline void MapDestroy(MapBase<K, V>* s, IAllocator* p);

template<typename K, typename V>
inline u32 MapCap(MapBase<K, V>* s);

template<typename K, typename V>
inline u32 MapSize(MapBase<K, V>* s);

template<typename K, typename V>
inline void _MapRehash(MapBase<K, V>* s, IAllocator* p, u32 size);

template<typename K, typename V>
inline MapResult<K, V> _MapInsertHashed(MapBase<K ,V>* s, const K& key, const V& val, u64 hash);

template<typename K, typename V>
[[nodiscard]] inline MapResult<K, V> _MapSearchHashed(MapBase<K, V>* s, const K& key, u64 keyHash);

template<typename K, typename V>
struct MapBase
{
    VecBase<MapBucket<K, V>> aBuckets {};
    f32 maxLoadFactor {};
    u32 nOccupied {};

    MapBase() = default;
    MapBase(IAllocator* pAllocator, u32 prealloc = SIZE_MIN);

    struct It
    {
        MapBase* s {};
        u32 i = 0;

        It(MapBase* _s, u32 _i) : s(_s), i(_i) {}

        KeyVal<K, V>& operator*() { return *(KeyVal<K, V>*)&s->aBuckets[i]; }
        KeyVal<K, V>* operator->() { return (KeyVal<K, V>*)&s->aBuckets[i]; }

        It operator++()
        {
            i = MapNextI(s, i);
            return {s, i};
        }
        It operator++(int) { auto* tmp = s++; return tmp; }

        friend bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend bool operator!=(const It& l, const It& r) { return l.i != r.i; }
    };

    It begin() { return {this, MapFirstI(this)}; }
    It end() { return {this, NPOS}; }

    const It begin() const { return {this, MapFirstI(this)}; }
    const It end() const { return {this, NPOS}; }
};

template<typename K, typename V>
inline u32
MapIdx(MapBase<K, V>* s, KeyVal<K, V>* p)
{
    auto r = (MapBucket<K, V>*)p - &s->aBuckets[0];
    assert(r < VecCap(&s->aBuckets));
    return r;
}

template<typename K, typename V>
inline u32
MapIdx(MapBase<K, V>* s, MapResult<K, V> res)
{
    auto idx = res.pData - &s->aBuckets[0];
    assert(idx < VecCap(&s->aBuckets));
    return idx;
}

template<typename K, typename V>
inline u32
MapFirstI(MapBase<K, V>* s)
{
    u32 i = 0;
    while (i < VecCap(&s->aBuckets) && !s->aBuckets[i].bOccupied)
        ++i;

    if (i >= VecCap(&s->aBuckets)) i = NPOS;

    return i;
}

template<typename K, typename V>
inline u32
MapNextI(MapBase<K, V>* s, u32 i)
{
    do ++i;
    while (i < VecCap(&s->aBuckets) && !s->aBuckets[i].bOccupied);

    if (i >= VecCap(&s->aBuckets)) i = NPOS;

    return i;
}

template<typename K, typename V>
inline f32
MapLoadFactor(MapBase<K, V>* s)
{
    return f32(s->nOccupied) / f32(VecCap(&s->aBuckets));
}

template<typename K, typename V>
inline MapResult<K, V>
MapInsert(MapBase<K ,V>* s, IAllocator* p, const K& key, const V& val)
{
    u64 keyHash = hash::func(key);

    if (VecCap(&s->aBuckets) == 0) *s = {p};
    else if (MapLoadFactor(s) >= s->maxLoadFactor)
        _MapRehash(s, p, VecCap(&s->aBuckets) * 2);

    return _MapInsertHashed(s, key, val, keyHash);
}

template<typename K, typename V>
[[nodiscard]] inline MapResult<K, V>
MapSearch(MapBase<K, V>* s, const K& key)
{
    u64 keyHash = hash::func(key);
    return _MapSearchHashed(s, key, keyHash);
}

template<typename K, typename V>
inline void
MapRemove(MapBase<K, V>*s, u32 i)
{
    s->aBuckets[i].bDeleted = true;
    s->aBuckets[i].bOccupied = false;
    --s->nOccupied;
}

template<typename K, typename V>
inline void
MapRemove(MapBase<K, V>*s, const K& key)
{
    auto f = MapSearch<K, V>(s, key);
    assert(f && "[Map]: not found");
    MapRemove<K, V>(s, MapIdx(s, f));
}

template<typename K, typename V>
inline MapResult<K, V>
MapTryInsert(MapBase<K, V>* s, IAllocator* p, const K& key, const V& val)
{
    auto f = MapSearch<K, V>(s, key);
    if (f)
    {
        f.eStatus = MAP_RESULT_STATUS::FOUND;
        return f;
    }
    else return MapInsert<K, V>(s, p, key, val);
}

template<typename K, typename V>
inline void
MapDestroy(MapBase<K, V>* s, IAllocator* p)
{
    VecDestroy(&s->aBuckets, p);
}

template<typename K, typename V>
inline u32 MapCap(MapBase<K, V>* s)
{
    return VecCap(&s->aBuckets);
}

template<typename K, typename V>
inline u32 MapSize(MapBase<K, V>* s)
{
    return s->nOccupied;
}

template<typename K, typename V>
inline void
_MapRehash(MapBase<K, V>* s, IAllocator* p, u32 size)
{
    auto mNew = MapBase<K, V>(p, size);

    for (u32 i = 0; i < VecCap(&s->aBuckets); ++i)
        if (s->aBuckets[i].bOccupied)
            MapInsert<K, V>(&mNew, p, s->aBuckets[i].key, s->aBuckets[i].val);

    MapDestroy<K, V>(s, p);
    *s = mNew;
}

template<typename K, typename V>
inline MapResult<K, V>
_MapInsertHashed(MapBase<K ,V>* s, const K& key, const V& val, u64 keyHash)
{
    u32 idx = u32(keyHash % VecCap(&s->aBuckets));

    while (s->aBuckets[idx].bOccupied)
    {
        ++idx;
        if (idx >= VecCap(&s->aBuckets)) idx = 0;
    }

    s->aBuckets[idx].key = key;
    s->aBuckets[idx].val = val;
    s->aBuckets[idx].bOccupied = true;
    s->aBuckets[idx].bDeleted = false;
    ++s->nOccupied;

    return {
        .pData = &s->aBuckets[idx],
        .hash = keyHash,
        .eStatus = MAP_RESULT_STATUS::INSERTED
    };
}

template<typename K, typename V>
[[nodiscard]] inline MapResult<K, V>
_MapSearchHashed(MapBase<K, V>* s, const K& key, u64 keyHash)
{
    MapResult<K, V> res {.eStatus = MAP_RESULT_STATUS::NOT_FOUND};

    if (s->nOccupied == 0) return res;

    auto& aBuckets = s->aBuckets;

    u32 idx = u32(keyHash % u64(VecCap(&aBuckets)));
    res.hash = keyHash;

    while (aBuckets[idx].bOccupied || aBuckets[idx].bDeleted)
    {
        if (!aBuckets[idx].bDeleted && aBuckets[idx].key == key)
        {
            res.pData = &aBuckets[idx];
            res.eStatus = MAP_RESULT_STATUS::FOUND;
            break;
        }

        ++idx;
        if (idx >= VecCap(&aBuckets)) idx = 0;
    }

    return res;
}


template<typename K, typename V>
MapBase<K, V>::MapBase(IAllocator* pAllocator, u32 prealloc)
    : aBuckets(pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV),
      maxLoadFactor(MAP_DEFAULT_LOAD_FACTOR)
{
    VecSetSize(&aBuckets, pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV);
    memset(aBuckets.pData, 0, sizeof(aBuckets[0]) * VecSize(&aBuckets));
}

template<typename K, typename V>
struct Map
{
    MapBase<K, V> base {};
    IAllocator* pA {};

    Map() = default;
    Map(IAllocator* pAlloc, u32 prealloc = SIZE_MIN)
        : base(pAlloc, prealloc), pA(pAlloc) {}

    MapBase<K, V>::It begin() { return base.begin(); }
    MapBase<K, V>::It end() { return base.end(); }

    const MapBase<K, V>::It begin() const { return base.begin(); }
    const MapBase<K, V>::It end() const { return base.end(); }
};

template<typename K, typename V>
inline u32 MapIdx(Map<K, V>* s, MapResult<K, V> res) { return MapIdx<K, V>(&s->base, res); }

template<typename K, typename V>
inline u32 MapFirstI(Map<K, V>* s) { return MapFirstI<K, V>(&s->base); }

template<typename K, typename V>
inline u32 MapNextI(Map<K, V>* s, u32 i) { return MapNextI<K, V>(&s->base, i); }

template<typename K, typename V>
inline f32 MapLoadFactor(Map<K, V>* s) { return MapLoadFactor<K, V>(&s->base); }

template<typename K, typename V>
inline MapResult<K, V> MapInsert(Map<K ,V>* s, const K& key, const V& val) { return MapInsert<K, V>(&s->base, s->pA, key, val); }

template<typename K, typename V>
[[nodiscard]] inline MapResult<K, V> MapSearch(Map<K, V>* s, const K& key) { return MapSearch<K, V>(&s->base, key); }

template<typename K, typename V>
inline void MapRemove(Map<K, V>*s, u32 i) { MapRemove<K, V>(&s->base, i); }

template<typename K, typename V>
inline void MapRemove(Map<K, V>*s, const K& key) { MapRemove<K, V>(&s->base, key); }

template<typename K, typename V>
inline MapResult<K, V> MapTryInsert(Map<K, V>* s, const K& key, const V& val) { return MapTryInsert<K, V>(&s->base, s->pA, key, val); }

template<typename K, typename V>
inline void MapDestroy(Map<K, V>* s) { MapDestroy<K, V>(&s->base, s->pA); }

template<typename K, typename V>
inline u32 MapCap(Map<K, V>* s) { return MapCap<K, V>(&s->base); }

template<typename K, typename V>
inline u32 MapSize(Map<K, V>* s) { return MapSize<K, V>(&s->base); }

namespace print
{

inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, MAP_RESULT_STATUS eStatus)
{
    ctx.fmt = "{}";
    ctx.fmtIdx = 0;
    constexpr String map[] {
        "FOUND", "NOT_FOUND", "INSERTED"
    };

    auto statusIdx = std::underlying_type_t<MAP_RESULT_STATUS>(eStatus);
    assert(statusIdx < utils::size(map) && "out of range enum");
    return printArgs(ctx, map[statusIdx]);
}

template<typename K, typename V>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const MapBucket<K, V>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.key, x.val);
}

template<typename K, typename V>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const KeyVal<K, V>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.key, x.val);
}

} /* namespace print */

} /* namespace adt */

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

    constexpr operator bool() const
    {
        return this->pData != nullptr;
    }

    constexpr const KeyVal<K, V>&
    getData() const
    {
        assert(eStatus != MAP_RESULT_STATUS::NOT_FOUND && "[Map]: not found");
        return *(KeyVal<K, V>*)pData;
    }
};

template<typename K, typename V>
struct MapBase
{
    VecBase<MapBucket<K, V>> aBuckets {};
    f32 maxLoadFactor {};
    u32 nOccupied {};

    MapBase() = default;
    MapBase(IAllocator* pAllocator, u32 prealloc = SIZE_MIN);

    [[nodiscard]] u32 idx(KeyVal<K, V>* p) const;

    [[nodiscard]] u32 idx(MapResult<K, V> res) const;

    [[nodiscard]] u32 firstI() const;

    [[nodiscard]] u32 nextI(u32 i) const;

    [[nodiscard]] f32 loadFactor() const;

    MapResult<K, V> insert(IAllocator* p, const K& key, const V& val);

    [[nodiscard]] MapResult<K, V> search(const K& key);

    void remove(u32 i);

    void remove(const K& key);

    [[nodiscard]] MapResult<K, V> tryInsert(IAllocator* p, const K& key, const V& val);

    void destroy(IAllocator* p);

    [[nodiscard]] u32 getCap() const;

    [[nodiscard]] u32 getSize() const;

    void rehash(IAllocator* p, u32 size);

    MapResult<K, V> insertHashed(const K& key, const V& val, u64 hash);

    [[nodiscard]] MapResult<K, V> searchHashed(const K& key, u64 keyHash);

    struct It
    {
        MapBase* s {};
        u32 i = 0;

        It(MapBase* _s, u32 _i) : s(_s), i(_i) {}

        KeyVal<K, V>& operator*() { return *(KeyVal<K, V>*)&s->aBuckets[i]; }
        KeyVal<K, V>* operator->() { return (KeyVal<K, V>*)&s->aBuckets[i]; }

        It operator++()
        {
            i = s->nextI(i);
            return {s, i};
        }
        It operator++(int) { auto* tmp = s++; return tmp; }

        friend bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend bool operator!=(const It& l, const It& r) { return l.i != r.i; }
    };

    It begin() { return {this, this->firstI()}; }
    It end() { return {this, NPOS}; }

    const It begin() const { return {this, this->firstI()}; }
    const It end() const { return {this, NPOS}; }
};

template<typename K, typename V>
inline u32
MapBase<K, V>::idx(KeyVal<K, V>* p) const
{
    auto r = (MapBucket<K, V>*)p - &this->aBuckets[0];
    assert(r < this->aBuckets.getCap());
    return r;
}

template<typename K, typename V>
inline u32
MapBase<K, V>::idx(MapResult<K, V> res) const
{
    auto idx = res.pData - &this->aBuckets[0];
    assert(idx < this->aBuckets.getCap());
    return idx;
}

template<typename K, typename V>
inline u32
MapBase<K, V>::firstI() const
{
    u32 i = 0;
    while (i < this->aBuckets.getCap() && !this->aBuckets[i].bOccupied)
        ++i;

    if (i >= this->aBuckets.getCap()) i = NPOS;

    return i;
}

template<typename K, typename V>
inline u32
MapBase<K, V>::nextI(u32 i) const
{
    do ++i;
    while (i < this->aBuckets.getCap() && !this->aBuckets[i].bOccupied);

    if (i >= this->aBuckets.getCap()) i = NPOS;

    return i;
}

template<typename K, typename V>
inline f32
MapBase<K, V>::loadFactor() const
{
    return f32(this->nOccupied) / f32(this->aBuckets.getCap());
}

template<typename K, typename V>
inline MapResult<K, V>
MapBase<K, V>::insert(IAllocator* p, const K& key, const V& val)
{
    u64 keyHash = hash::func(key);

    if (this->aBuckets.getCap() == 0) *this = {p};
    else if (this->loadFactor() >= this->maxLoadFactor)
        this->rehash(p, this->aBuckets.getCap() * 2);

    return this->insertHashed(key, val, keyHash);
}

template<typename K, typename V>
[[nodiscard]] inline MapResult<K, V>
MapBase<K, V>::search(const K& key)
{
    u64 keyHash = hash::func(key);
    return this->searchHashed(key, keyHash);
}

template<typename K, typename V>
inline void
MapBase<K, V>::remove(u32 i)
{
    this->aBuckets[i].bDeleted = true;
    this->aBuckets[i].bOccupied = false;
    --this->nOccupied;
}

template<typename K, typename V>
inline void
MapBase<K, V>::remove(const K& key)
{
    auto f = this->search(key);
    assert(f && "[Map]: not found");
    this->remove(this->idx(f));
}

template<typename K, typename V>
inline MapResult<K, V>
MapBase<K, V>::tryInsert(IAllocator* p, const K& key, const V& val)
{
    auto f = this->search(key);
    if (f)
    {
        f.eStatus = MAP_RESULT_STATUS::FOUND;
        return f;
    }
    else return this->insert(p, key, val);
}

template<typename K, typename V>
inline void
MapBase<K, V>::destroy(IAllocator* p)
{
    this->aBuckets.destroy(p);
}

template<typename K, typename V>
inline u32 MapBase<K, V>::getCap() const
{
    return this->aBuckets.getCap();
}

template<typename K, typename V>
inline u32 MapBase<K, V>::getSize() const
{
    return this->nOccupied;
}

template<typename K, typename V>
inline void
MapBase<K, V>::rehash(IAllocator* p, u32 size)
{
    auto mNew = MapBase<K, V>(p, size);

    for (u32 i = 0; i < this->aBuckets.getCap(); ++i)
        if (this->aBuckets[i].bOccupied)
            mNew.insert(p, this->aBuckets[i].key, this->aBuckets[i].val);

    this->destroy(p);
    *this = mNew;
}

template<typename K, typename V>
inline MapResult<K, V>
MapBase<K, V>::insertHashed(const K& key, const V& val, u64 keyHash)
{
    u32 idx = u32(keyHash % this->aBuckets.getCap());

    while (this->aBuckets[idx].bOccupied)
        ++idx %= this->aBuckets.getCap();

    this->aBuckets[idx].key = key;
    this->aBuckets[idx].val = val;
    this->aBuckets[idx].bOccupied = true;
    this->aBuckets[idx].bDeleted = false;
    ++this->nOccupied;

    return {
        .pData = &this->aBuckets[idx],
        .hash = keyHash,
        .eStatus = MAP_RESULT_STATUS::INSERTED
    };
}

template<typename K, typename V>
[[nodiscard]] inline MapResult<K, V>
MapBase<K, V>::searchHashed(const K& key, u64 keyHash)
{
    MapResult<K, V> res {.eStatus = MAP_RESULT_STATUS::NOT_FOUND};

    if (this->nOccupied == 0) return res;

    auto& aBuckets = this->aBuckets;

    u32 idx = u32(keyHash % u64(aBuckets.getCap()));
    res.hash = keyHash;

    while (aBuckets[idx].bOccupied || aBuckets[idx].bDeleted)
    {
        if (!aBuckets[idx].bDeleted && aBuckets[idx].key == key)
        {
            res.pData = &aBuckets[idx];
            res.eStatus = MAP_RESULT_STATUS::FOUND;
            break;
        }

        ++idx %= aBuckets.getCap();
    }

    return res;
}


template<typename K, typename V>
MapBase<K, V>::MapBase(IAllocator* pAllocator, u32 prealloc)
    : aBuckets(pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV),
      maxLoadFactor(MAP_DEFAULT_LOAD_FACTOR)
{
    aBuckets.setSize(pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV);
    memset(aBuckets.pData, 0, sizeof(aBuckets[0]) * aBuckets.getSize());
}

template<typename K, typename V>
struct Map
{
    MapBase<K, V> base {};
    IAllocator* pAlloc {};

    Map() = default;
    Map(IAllocator* _pAlloc, u32 prealloc = SIZE_MIN)
        : base(_pAlloc, prealloc), pAlloc(_pAlloc) {}

    MapBase<K, V>::It begin() { return base.begin(); }
    MapBase<K, V>::It end() { return base.end(); }

    const MapBase<K, V>::It begin() const { return base.begin(); }
    const MapBase<K, V>::It end() const { return base.end(); }

    [[nodiscard]] inline u32 idx(MapResult<K, V> res) const { return base.idx(res); }

    [[nodiscard]] inline u32 firstI() const { return base.firstI(); }

    [[nodiscard]] inline u32 nextI(u32 i) const { return base.nextI(i); }

    [[nodiscard]] inline f32 loadFactor() const { return base.loadFactor(); }

    inline MapResult<K, V> insert(const K& key, const V& val) { return base.insert(pAlloc, key, val); }

    [[nodiscard]] inline MapResult<K, V> search(const K& key) { return base.search(key); }

    inline void remove(u32 i) { base.remove(i); }

    inline void remove(const K& key) { base.remove(key); }

    [[nodiscard]] inline MapResult<K, V> tryInsert(const K& key, const V& val) { return base.tryInsert(pAlloc, key, val); }

    inline void destroy() { base.destroy(pAlloc); }

    [[nodiscard]] inline u32 getCap() const { return base.getCap(); }

    [[nodiscard]] inline u32 getSize() const { return base.getSize(); }
};

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

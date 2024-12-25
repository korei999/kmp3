/* Hashmap with linear probing.
 * For custom hash function add template<> hash::func(const KeyType& x),
 * and bool operator==(const KeyType& other) */

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
        return pData != nullptr;
    }

    constexpr
    const KeyVal<K, V>&
    getData() const
    {
        assert(eStatus != MAP_RESULT_STATUS::NOT_FOUND && "[Map]: not found");
        return *(KeyVal<K, V>*)pData;
    }
};

template<typename K, typename V>
struct MapBase
{
    VecBase<MapBucket<K, V>> m_aBuckets {};
    f32 m_maxLoadFactor {};
    u32 m_nOccupied {};

    /* */

    MapBase() = default;
    MapBase(IAllocator* pAllocator, u32 prealloc = SIZE_MIN);

    /* */

    [[nodiscard]] bool empty() const { return m_nOccupied == 0; }
    [[nodiscard]] u32 idx(KeyVal<K, V>* p) const;
    [[nodiscard]] u32 idx(MapResult<K, V> res) const;
    [[nodiscard]] u32 firstI() const;
    [[nodiscard]] u32 nextI(u32 i) const;
    [[nodiscard]] f32 loadFactor() const;
    MapResult<K, V> insert(IAllocator* p, const K& key, const V& val);
    [[nodiscard]] MapResult<K, V> search(const K& key);
    void remove(u32 i);
    void remove(const K& key);
    MapResult<K, V> tryInsert(IAllocator* p, const K& key, const V& val);
    void destroy(IAllocator* p);
    [[nodiscard]] u32 getCap() const;
    [[nodiscard]] u32 getSize() const;
    void rehash(IAllocator* p, u32 size);
    MapResult<K, V> insertHashed(const K& key, const V& val, u64 hash);
    [[nodiscard]] MapResult<K, V> searchHashed(const K& key, u64 keyHash);

    /* */

    struct It
    {
        MapBase* s {};
        u32 i = 0;

        It(MapBase* _s, u32 _i) : s(_s), i(_i) {}

        KeyVal<K, V>& operator*() { return *(KeyVal<K, V>*)&s->m_aBuckets[i]; }
        KeyVal<K, V>* operator->() { return (KeyVal<K, V>*)&s->m_aBuckets[i]; }

        It operator++()
        {
            i = s->nextI(i);
            return {s, i};
        }
        It operator++(int) { auto* tmp = s++; return tmp; }

        friend bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend bool operator!=(const It& l, const It& r) { return l.i != r.i; }
    };

    It begin() { return {this, firstI()}; }
    It end() { return {this, NPOS}; }

    const It begin() const { return {this, firstI()}; }
    const It end() const { return {this, NPOS}; }
};

template<typename K, typename V>
inline u32
MapBase<K, V>::idx(KeyVal<K, V>* p) const
{
    auto r = (MapBucket<K, V>*)p - &m_aBuckets[0];
    assert(r < m_aBuckets.getCap());
    return r;
}

template<typename K, typename V>
inline u32
MapBase<K, V>::idx(MapResult<K, V> res) const
{
    auto idx = res.pData - &m_aBuckets[0];
    assert(idx < m_aBuckets.getCap());
    return idx;
}

template<typename K, typename V>
inline u32
MapBase<K, V>::firstI() const
{
    u32 i = 0;
    while (i < m_aBuckets.getCap() && !m_aBuckets[i].bOccupied)
        ++i;

    if (i >= m_aBuckets.getCap()) i = NPOS;

    return i;
}

template<typename K, typename V>
inline u32
MapBase<K, V>::nextI(u32 i) const
{
    do ++i;
    while (i < m_aBuckets.getCap() && !m_aBuckets[i].bOccupied);

    if (i >= m_aBuckets.getCap()) i = NPOS;

    return i;
}

template<typename K, typename V>
inline f32
MapBase<K, V>::loadFactor() const
{
    return f32(m_nOccupied) / f32(m_aBuckets.getCap());
}

template<typename K, typename V>
inline MapResult<K, V>
MapBase<K, V>::insert(IAllocator* p, const K& key, const V& val)
{
    u64 keyHash = hash::func(key);

    if (m_aBuckets.getCap() == 0) *this = {p};
    else if (loadFactor() >= m_maxLoadFactor)
        rehash(p, m_aBuckets.getCap() * 2);

    return insertHashed(key, val, keyHash);
}

template<typename K, typename V>
[[nodiscard]] inline MapResult<K, V>
MapBase<K, V>::search(const K& key)
{
    u64 keyHash = hash::func(key);
    return searchHashed(key, keyHash);
}

template<typename K, typename V>
inline void
MapBase<K, V>::remove(u32 i)
{
    m_aBuckets[i].bDeleted = true;
    m_aBuckets[i].bOccupied = false;
    --m_nOccupied;
}

template<typename K, typename V>
inline void
MapBase<K, V>::remove(const K& key)
{
    auto f = search(key);
    assert(f && "[Map]: not found");
    remove(idx(f));
}

template<typename K, typename V>
inline MapResult<K, V>
MapBase<K, V>::tryInsert(IAllocator* p, const K& key, const V& val)
{
    auto f = search(key);
    if (f)
    {
        f.eStatus = MAP_RESULT_STATUS::FOUND;
        return f;
    }
    else return insert(p, key, val);
}

template<typename K, typename V>
inline void
MapBase<K, V>::destroy(IAllocator* p)
{
    m_aBuckets.destroy(p);
}

template<typename K, typename V>
inline u32 MapBase<K, V>::getCap() const
{
    return m_aBuckets.getCap();
}

template<typename K, typename V>
inline u32 MapBase<K, V>::getSize() const
{
    return m_nOccupied;
}

template<typename K, typename V>
inline void
MapBase<K, V>::rehash(IAllocator* p, u32 size)
{
    auto mNew = MapBase<K, V>(p, size);

    for (u32 i = 0; i < m_aBuckets.getCap(); ++i)
        if (m_aBuckets[i].bOccupied)
            mNew.insert(p, m_aBuckets[i].key, m_aBuckets[i].val);

    destroy(p);
    *this = mNew;
}

template<typename K, typename V>
inline MapResult<K, V>
MapBase<K, V>::insertHashed(const K& key, const V& val, u64 keyHash)
{
    u32 idx = u32(keyHash % m_aBuckets.getCap());

    while (m_aBuckets[idx].bOccupied)
    {
        ++idx;
        if (idx >= m_aBuckets.getCap())
            idx = 0;
    }

    new(&m_aBuckets[idx].key) K(key);
    new(&m_aBuckets[idx].val) V(val);

    m_aBuckets[idx].bOccupied = true;
    m_aBuckets[idx].bDeleted = false;

    ++m_nOccupied;

    return {
        .pData = &m_aBuckets[idx],
        .hash = keyHash,
        .eStatus = MAP_RESULT_STATUS::INSERTED
    };
}

template<typename K, typename V>
[[nodiscard]] inline MapResult<K, V>
MapBase<K, V>::searchHashed(const K& key, u64 keyHash)
{
    MapResult<K, V> res {.eStatus = MAP_RESULT_STATUS::NOT_FOUND};

    if (m_nOccupied == 0) return res;

    auto& aBuckets = m_aBuckets;

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

        ++idx;
        if (idx >= aBuckets.getCap())
            idx = 0;
    }

    return res;
}


template<typename K, typename V>
MapBase<K, V>::MapBase(IAllocator* pAllocator, u32 prealloc)
    : m_aBuckets(pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV),
      m_maxLoadFactor(MAP_DEFAULT_LOAD_FACTOR)
{
    m_aBuckets.setSize(pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV);
    memset(m_aBuckets.data(), 0, sizeof(m_aBuckets[0]) * m_aBuckets.getSize());
}

template<typename K, typename V>
struct Map
{
    MapBase<K, V> base {};

    /* */

    IAllocator* m_pAlloc {};

    /* */

    Map() = default;
    Map(IAllocator* _pAlloc, u32 prealloc = SIZE_MIN)
        : base(_pAlloc, prealloc), m_pAlloc(_pAlloc) {}

    /* */

    [[nodiscard]] bool empty() const { return base.empty(); }
    [[nodiscard]] u32 idx(MapResult<K, V> res) const { return base.idx(res); }
    [[nodiscard]] u32 firstI() const { return base.firstI(); }
    [[nodiscard]] u32 nextI(u32 i) const { return base.nextI(i); }
    [[nodiscard]] f32 loadFactor() const { return base.loadFactor(); }
    MapResult<K, V> insert(const K& key, const V& val) { return base.insert(m_pAlloc, key, val); }
    [[nodiscard]] MapResult<K, V> search(const K& key) { return base.search(key); }
    void remove(u32 i) { base.remove(i); }
    void remove(const K& key) { base.remove(key); }
    MapResult<K, V> tryInsert(const K& key, const V& val) { return base.tryInsert(m_pAlloc, key, val); }
    void destroy() { base.destroy(m_pAlloc); }
    [[nodiscard]] u32 getCap() const { return base.getCap(); }
    [[nodiscard]] u32 getSize() const { return base.getSize(); }

    /* */

    MapBase<K, V>::It begin() { return base.begin(); }
    MapBase<K, V>::It end() { return base.end(); }

    const MapBase<K, V>::It begin() const { return base.begin(); }
    const MapBase<K, V>::It end() const { return base.end(); }
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

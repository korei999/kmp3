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
    usize hash {};
    MAP_RESULT_STATUS eStatus {};

    constexpr operator bool() const
    {
        return pData != nullptr;
    }

    constexpr
    const KeyVal<K, V>&
    data() const
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
    ssize m_nOccupied {};

    /* */

    MapBase() = default;
    MapBase(IAllocator* pAllocator, ssize prealloc = SIZE_MIN);

    /* */

    [[nodiscard]] bool empty() const { return m_nOccupied == 0; }

    [[nodiscard]] ssize idx(KeyVal<K, V>* p) const;

    [[nodiscard]] ssize idx(MapResult<K, V> res) const;

    [[nodiscard]] ssize firstI() const;

    [[nodiscard]] ssize nextI(ssize i) const;

    [[nodiscard]] f32 loadFactor() const;

    MapResult<K, V> insert(IAllocator* p, const K& key, const V& val);

    template<typename ...ARGS> requires(std::is_constructible_v<V, ARGS...>)
        MapResult<K, V> emplace(IAllocator* p, const K& key, ARGS&&... args);

    [[nodiscard]] MapResult<K, V> search(const K& key);

    void remove(ssize i);

    void remove(const K& key);

    MapResult<K, V> tryInsert(IAllocator* p, const K& key, const V& val);

    void destroy(IAllocator* p);

    [[nodiscard]] ssize getCap() const;

    [[nodiscard]] ssize getSize() const;

    void rehash(IAllocator* p, ssize size);

    MapResult<K, V> insertHashed(const K& key, const V& val, usize hash);

    [[nodiscard]] MapResult<K, V> searchHashed(const K& key, usize keyHash);

    void zeroOut();

    /* */

private:
    ssize getInsertionIdx(usize hash) const;

    /* */

public:
    struct It
    {
        MapBase* s {};
        ssize i = 0;

        It(MapBase* _s, ssize _i) : s(_s), i(_i) {}

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
inline ssize
MapBase<K, V>::idx(KeyVal<K, V>* p) const
{
    auto r = (MapBucket<K, V>*)p - &m_aBuckets[0];
    assert(r < m_aBuckets.getCap());
    return r;
}

template<typename K, typename V>
inline ssize
MapBase<K, V>::idx(MapResult<K, V> res) const
{
    auto idx = res.pData - &m_aBuckets[0];
    assert(idx < m_aBuckets.getCap());
    return idx;
}

template<typename K, typename V>
inline ssize
MapBase<K, V>::firstI() const
{
    ssize i = 0;
    while (i < m_aBuckets.getCap() && !m_aBuckets[i].bOccupied)
        ++i;

    if (i >= m_aBuckets.getCap()) i = NPOS;

    return i;
}

template<typename K, typename V>
inline ssize
MapBase<K, V>::nextI(ssize i) const
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
    if (m_aBuckets.getCap() == 0) *this = {p};
    else if (loadFactor() >= m_maxLoadFactor)
        rehash(p, m_aBuckets.getCap() * 2);

    usize keyHash = hash::func(key);

    return insertHashed(key, val, keyHash);
}

template<typename K, typename V>
template<typename ...ARGS> requires(std::is_constructible_v<V, ARGS...>)
inline MapResult<K, V>
MapBase<K, V>::emplace(IAllocator* p, const K& key, ARGS&&... args)
{
    if (m_aBuckets.getCap() == 0) *this = {p};
    else if (loadFactor() >= m_maxLoadFactor)
        rehash(p, m_aBuckets.getCap() * 2);

    usize keyHash = hash::func(key);
    ssize idx = getInsertionIdx(keyHash);

    new(&m_aBuckets[idx].key) K(key);
    new(&m_aBuckets[idx].val) V(std::forward<ARGS>(args)...);

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
MapBase<K, V>::search(const K& key)
{
    usize keyHash = hash::func(key);
    return searchHashed(key, keyHash);
}

template<typename K, typename V>
inline void
MapBase<K, V>::remove(ssize i)
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
inline ssize MapBase<K, V>::getCap() const
{
    return m_aBuckets.getCap();
}

template<typename K, typename V>
inline ssize MapBase<K, V>::getSize() const
{
    return m_nOccupied;
}

template<typename K, typename V>
inline void
MapBase<K, V>::rehash(IAllocator* p, ssize size)
{
    auto mNew = MapBase<K, V>(p, size);

    for (ssize i = 0; i < m_aBuckets.getCap(); ++i)
        if (m_aBuckets[i].bOccupied)
            mNew.insert(p, m_aBuckets[i].key, m_aBuckets[i].val);

    destroy(p);
    *this = mNew;
}

template<typename K, typename V>
inline MapResult<K, V>
MapBase<K, V>::insertHashed(const K& key, const V& val, usize keyHash)
{
    const ssize idx = getInsertionIdx(keyHash);

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
MapBase<K, V>::searchHashed(const K& key, usize keyHash)
{
    MapResult<K, V> res {.eStatus = MAP_RESULT_STATUS::NOT_FOUND};

    if (m_nOccupied == 0) return res;

    auto& aBuckets = m_aBuckets;

    ssize idx = ssize(keyHash % usize(aBuckets.getCap()));
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
inline void
MapBase<K, V>::zeroOut()
{
    m_aBuckets.zeroOut();
    m_nOccupied = 0;
}

template<typename K, typename V>
inline ssize
MapBase<K, V>::getInsertionIdx(usize hash) const
{
    ssize idx = ssize(hash % m_aBuckets.getCap());

    while (m_aBuckets[idx].bOccupied)
    {
        ++idx;
        if (idx >= m_aBuckets.getCap())
            idx = 0;
    }

    return idx;
}

template<typename K, typename V>
MapBase<K, V>::MapBase(IAllocator* pAllocator, ssize prealloc)
    : m_aBuckets(pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV),
      m_maxLoadFactor(MAP_DEFAULT_LOAD_FACTOR)
{
    m_aBuckets.setSize(pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV);
    m_aBuckets.zeroOut();
}

template<typename K, typename V>
struct Map
{
    MapBase<K, V> base {};

    /* */

    IAllocator* m_pAlloc {};

    /* */

    Map() = default;
    Map(IAllocator* _pAlloc, ssize prealloc = SIZE_MIN)
        : base(_pAlloc, prealloc), m_pAlloc(_pAlloc) {}

    /* */

    [[nodiscard]] bool empty() const { return base.empty(); }

    [[nodiscard]] ssize idx(MapResult<K, V> res) const { return base.idx(res); }

    [[nodiscard]] ssize firstI() const { return base.firstI(); }

    [[nodiscard]] ssize nextI(ssize i) const { return base.nextI(i); }

    [[nodiscard]] f32 loadFactor() const { return base.loadFactor(); }

    MapResult<K, V> insert(const K& key, const V& val) { return base.insert(m_pAlloc, key, val); }

    template<typename ...ARGS> requires(std::is_constructible_v<V, ARGS...>) MapResult<K, V> emplace(const K& key, ARGS&&... args)
    { return base.emplace(m_pAlloc, key, std::forward<ARGS>(args)...); };

    [[nodiscard]] MapResult<K, V> search(const K& key) { return base.search(key); }

    void remove(ssize i) { base.remove(i); }

    void remove(const K& key) { base.remove(key); }

    MapResult<K, V> tryInsert(const K& key, const V& val) { return base.tryInsert(m_pAlloc, key, val); }

    void destroy() { base.destroy(m_pAlloc); }

    [[nodiscard]] ssize getCap() const { return base.getCap(); }

    [[nodiscard]] ssize getSize() const { return base.getSize(); }

    void zeroOut() { base.zeroOut(); }

    /* */

    MapBase<K, V>::It begin() { return base.begin(); }
    MapBase<K, V>::It end() { return base.end(); }

    const MapBase<K, V>::It begin() const { return base.begin(); }
    const MapBase<K, V>::It end() const { return base.end(); }
};

namespace print
{

inline ssize
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
inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const MapBucket<K, V>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.key, x.val);
}

template<typename K, typename V>
inline ssize
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const KeyVal<K, V>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.key, x.val);
}

} /* namespace print */

} /* namespace adt */

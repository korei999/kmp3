/* Hashmap with linear probing.
 * For custom hash function add template<> hash::func(const KeyType& x), (or specify in the template argument)
 * and bool operator==(const KeyType& other) */

#pragma once

#include "Vec.hh"
#include "hash.hh"
#include "print.hh"
#include "defer.hh"

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

    /* */

    explicit constexpr operator bool() const { return pData != nullptr; }

    /* */

    [[nodiscard]] KeyVal<K, V>&
    data()
    {
        ADT_ASSERT(eStatus != MAP_RESULT_STATUS::NOT_FOUND, "not found");
        return *(KeyVal<K, V>*)pData;
    }

    [[nodiscard]] const KeyVal<K, V>&
    data() const
    {
        ADT_ASSERT(eStatus != MAP_RESULT_STATUS::NOT_FOUND, "not found");
        return *(KeyVal<K, V>*)pData;
    }

    [[nodiscard]] K& key() { return data().key; }
    [[nodiscard]] const K& key() const { return data().key; }

    [[nodiscard]] V& value() { return data().val; }
    [[nodiscard]] const V& value() const { return data().val; }

    [[nodiscard]] const V
    valueOr(V&& v) const
    {
        if (eStatus != MAP_RESULT_STATUS::NOT_FOUND)
            return value();
        else return std::forward<V>(v);
    }
};

template<typename K, typename V, usize (*FN_HASH)(const K&) = hash::func<K>>
struct Map
{
    Vec<MapBucket<K, V>> m_vBuckets {};
    ssize m_nOccupied {};
    f32 m_maxLoadFactor {};

    /* */

    Map() = default;
    Map(IAllocator* pAllocator, ssize prealloc = SIZE_MIN);

    /* */

    [[nodiscard]] bool empty() const { return m_nOccupied <= 0; }

    [[nodiscard]] ssize idx(const KeyVal<K, V>* p) const;

    [[nodiscard]] ssize idx(const MapResult<K, V> res) const;

    [[nodiscard]] ssize firstI() const;

    [[nodiscard]] ssize nextI(ssize i) const;

    [[nodiscard]] f32 getLoadFactor() const;

    MapResult<K, V> insert(IAllocator* p, const K& key, const V& val);

    template<typename ...ARGS> requires(std::is_constructible_v<V, ARGS...>)
        MapResult<K, V> emplace(IAllocator* p, const K& key, ARGS&&... args);

    [[nodiscard]] MapResult<K, V> search(const K& key);
    [[nodiscard]] const MapResult<K, V> search(const K& key) const;

    void remove(ssize i);

    void remove(const K& key);

    bool tryRemove(const K& key);

    MapResult<K, V> tryInsert(IAllocator* p, const K& key, const V& val);

    void destroy(IAllocator* p);

    [[nodiscard]] ssize cap() const;

    [[nodiscard]] ssize size() const;

    void rehash(IAllocator* p, ssize size);

    MapResult<K, V> insertHashed(const K& key, const V& val, usize hash);

    [[nodiscard]] MapResult<K, V> searchHashed(const K& key, usize keyHash) const;

    void zeroOut();

    /* */

protected:
    ssize getInsertionIdx(usize hash, const K& key) const;

    /* */

public:
    struct It
    {
        Map* s {};
        ssize i = 0;

        It(Map* _s, ssize _i) : s(_s), i(_i) {}

        KeyVal<K, V>& operator*() { return *(KeyVal<K, V>*)&s->m_vBuckets[i]; }
        KeyVal<K, V>* operator->() { return (KeyVal<K, V>*)&s->m_vBuckets[i]; }

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

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline ssize
Map<K, V, FN_HASH>::idx(const KeyVal<K, V>* p) const
{
    ssize r = (MapBucket<K, V>*)p - &m_vBuckets[0];
    ADT_ASSERT(r >= 0 && r < m_vBuckets.cap(), "out of range, r: {}, cap: {}", r, m_vBuckets.cap());
    return r;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline ssize
Map<K, V, FN_HASH>::idx(const MapResult<K, V> res) const
{
    ssize idx = res.pData - &m_vBuckets[0];
    ADT_ASSERT(idx >= 0 && idx < m_vBuckets.cap(), "out of range, r: {}, cap: {}", idx, m_vBuckets.cap());
    return idx;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline ssize
Map<K, V, FN_HASH>::firstI() const
{
    ssize i = 0;
    while (i < m_vBuckets.cap() && !m_vBuckets[i].bOccupied)
        ++i;

    if (i >= m_vBuckets.cap()) i = NPOS;

    return i;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline ssize
Map<K, V, FN_HASH>::nextI(ssize i) const
{
    do ++i;
    while (i < m_vBuckets.cap() && !m_vBuckets[i].bOccupied);

    if (i >= m_vBuckets.cap()) i = NPOS;

    return i;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline f32
Map<K, V, FN_HASH>::getLoadFactor() const
{
    return f32(m_nOccupied) / f32(m_vBuckets.cap());
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline MapResult<K, V>
Map<K, V, FN_HASH>::insert(IAllocator* p, const K& key, const V& val)
{
    if (m_vBuckets.cap() == 0)
        *this = {p};
    else if (getLoadFactor() >= m_maxLoadFactor)
        rehash(p, m_vBuckets.cap() * 2);

    usize keyHash = FN_HASH(key);

    return insertHashed(key, val, keyHash);
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
template<typename ...ARGS> requires(std::is_constructible_v<V, ARGS...>)
inline MapResult<K, V>
Map<K, V, FN_HASH>::emplace(IAllocator* p, const K& key, ARGS&&... args)
{
    if (m_vBuckets.cap() == 0)
        *this = {p};
    else if (getLoadFactor() >= m_maxLoadFactor)
        rehash(p, m_vBuckets.cap() * 2);

    usize keyHash = FN_HASH(key);
    ssize idx = getInsertionIdx(keyHash, key);
    auto& bucket = m_vBuckets[idx];

    new(&bucket.val) V(std::forward<ARGS>(args)...);

    /* replace same key */
    if (bucket.key == key)
    {
        return {
            .pData = &bucket,
            .hash = keyHash,
            .eStatus = MAP_RESULT_STATUS::FOUND,
        };
    }

    new(&bucket.key) K(key);

    bucket.bOccupied = true;
    bucket.bDeleted = false;

    ++m_nOccupied;

    return {
        .pData = &bucket,
        .hash = keyHash,
        .eStatus = MAP_RESULT_STATUS::INSERTED,
    };
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
[[nodiscard]] inline MapResult<K, V>
Map<K, V, FN_HASH>::search(const K& key)
{
    usize keyHash = FN_HASH(key);
    return searchHashed(key, keyHash);
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
[[nodiscard]] inline const MapResult<K, V>
Map<K, V, FN_HASH>::search(const K& key) const
{
    usize keyHash = FN_HASH(key);
    return searchHashed(key, keyHash);
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline void
Map<K, V, FN_HASH>::remove(ssize i)
{
    auto& bucket = m_vBuckets[i];

    bucket.key = {};
    bucket.val = {};
    bucket.bDeleted = true;
    bucket.bOccupied = false;

    --m_nOccupied;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline void
Map<K, V, FN_HASH>::remove(const K& key)
{
    auto found = search(key);
    ADT_ASSERT(found, "not found");
    remove(idx(found));
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline bool
Map<K, V, FN_HASH>::tryRemove(const K& key)
{
    auto found = search(key);
    if (found)
    {
        remove(idx(&found.data()));
        return true;
    }
    else
    {
        return false;
    }
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline MapResult<K, V>
Map<K, V, FN_HASH>::tryInsert(IAllocator* p, const K& key, const V& val)
{
    auto f = search(key);
    if (f)
    {
        f.eStatus = MAP_RESULT_STATUS::FOUND;
        return f;
    }
    else return insert(p, key, val);
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline void
Map<K, V, FN_HASH>::destroy(IAllocator* p)
{
    m_vBuckets.destroy(p);
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline ssize Map<K, V, FN_HASH>::cap() const
{
    return m_vBuckets.cap();
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline ssize Map<K, V, FN_HASH>::size() const
{
    return m_nOccupied;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline void
Map<K, V, FN_HASH>::rehash(IAllocator* p, ssize size)
{
    auto mNew = Map<K, V, FN_HASH>(p, size);

    for (const auto& [key, val] : *this)
        mNew.insert(p, key, val);

    destroy(p);
    *this = mNew;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline MapResult<K, V>
Map<K, V, FN_HASH>::insertHashed(const K& key, const V& val, usize keyHash)
{
    const ssize idx = getInsertionIdx(keyHash, key);
    auto& bucket = m_vBuckets[idx];

    new(&bucket.val) V(val);

    ADT_DEFER(
        bucket.bOccupied = true;
        bucket.bDeleted = false;
    );

    if (bucket.key == key)
    {
        return {
            .pData = &bucket,
            .hash = keyHash,
            .eStatus = MAP_RESULT_STATUS::FOUND,
        };
    }

    new(&bucket.key) K(key);

    ++m_nOccupied;

    return {
        .pData = &bucket,
        .hash = keyHash,
        .eStatus = MAP_RESULT_STATUS::INSERTED
    };
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
[[nodiscard]] inline MapResult<K, V>
Map<K, V, FN_HASH>::searchHashed(const K& key, usize keyHash) const
{
    MapResult<K, V> res {.eStatus = MAP_RESULT_STATUS::NOT_FOUND};

    if (m_nOccupied == 0)
    {
#ifndef NDEBUG
        print::err("Map::search: m_nOccupied: {}\n", m_nOccupied);
#endif
        return res;
    }

    ssize idx = ssize(keyHash % usize(m_vBuckets.cap()));
    res.hash = keyHash;

    while (m_vBuckets[idx].bOccupied || m_vBuckets[idx].bDeleted)
    {
        if (!m_vBuckets[idx].bDeleted && m_vBuckets[idx].key == key)
        {
            res.pData = const_cast<MapBucket<K, V>*>(&m_vBuckets[idx]);
            res.eStatus = MAP_RESULT_STATUS::FOUND;
            break;
        }

        ++idx;
        if (idx >= m_vBuckets.cap())
            idx = 0;
    }

    return res;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline void
Map<K, V, FN_HASH>::zeroOut()
{
    m_vBuckets.zeroOut();
    m_nOccupied = 0;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline ssize
Map<K, V, FN_HASH>::getInsertionIdx(usize hash, const K& key) const
{
    ssize idx = ssize(hash % m_vBuckets.cap());

    while (m_vBuckets[idx].bOccupied)
    {
        if (m_vBuckets[idx].key == key)
            break;

        ++idx;
        if (idx >= m_vBuckets.cap())
            idx = 0;
    }

    return idx;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
Map<K, V, FN_HASH>::Map(IAllocator* pAllocator, ssize prealloc)
    : m_vBuckets(pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV),
      m_maxLoadFactor(MAP_DEFAULT_LOAD_FACTOR)
{
    m_vBuckets.setSize(pAllocator, prealloc * MAP_DEFAULT_LOAD_FACTOR_INV);
    m_vBuckets.zeroOut();
}

template<typename K, typename V, usize (*FN_HASH)(const K&) = hash::func<K>>
struct MapManaged
{
    Map<K, V, FN_HASH> base {};

    /* */

    IAllocator* m_pAlloc {};

    /* */

    MapManaged() = default;
    MapManaged(IAllocator* _pAlloc, ssize prealloc = SIZE_MIN)
        : base(_pAlloc, prealloc), m_pAlloc(_pAlloc) {}

    /* */

    [[nodiscard]] bool empty() const { return base.empty(); }

    [[nodiscard]] ssize idx(MapResult<K, V> res) const { return base.idx(res); }

    [[nodiscard]] ssize firstI() const { return base.firstI(); }

    [[nodiscard]] ssize nextI(ssize i) const { return base.nextI(i); }

    [[nodiscard]] f32 getLoadFactor() const { return base.getLoadFactor(); }

    MapResult<K, V> insert(const K& key, const V& val) { return base.insert(m_pAlloc, key, val); }

    template<typename ...ARGS> requires(std::is_constructible_v<V, ARGS...>) MapResult<K, V> emplace(const K& key, ARGS&&... args)
    { return base.emplace(m_pAlloc, key, std::forward<ARGS>(args)...); };

    [[nodiscard]] MapResult<K, V> search(const K& key) { return base.search(key); }

    [[nodiscard]] const MapResult<K, V> search(const K& key) const { return base.search(key); }

    void remove(ssize i) { base.remove(i); }

    void remove(const K& key) { base.remove(key); }

    bool tryRemove(const K& key) { return base.tryRemove(key); }

    MapResult<K, V> tryInsert(const K& key, const V& val) { return base.tryInsert(m_pAlloc, key, val); }

    void destroy() { base.destroy(m_pAlloc); }

    [[nodiscard]] ssize cap() const { return base.cap(); }

    [[nodiscard]] ssize size() const { return base.size(); }

    void zeroOut() { base.zeroOut(); }

    /* */

    typename Map<K, V, FN_HASH>::It begin() { return base.begin(); }
    typename Map<K, V, FN_HASH>::It end() { return base.end(); }

    const typename Map<K, V, FN_HASH>::It begin() const { return base.begin(); }
    const typename Map<K, V, FN_HASH>::It end() const { return base.end(); }
};

namespace print
{

inline ssize
formatToContext(Context ctx, FormatArgs, MAP_RESULT_STATUS eStatus)
{
    ctx.fmt = "{}";
    ctx.fmtIdx = 0;
    constexpr StringView map[] {
        "FOUND", "NOT_FOUND", "INSERTED"
    };

    auto statusIdx = std::underlying_type_t<MAP_RESULT_STATUS>(eStatus);
    ADT_ASSERT(statusIdx >= 0 && statusIdx < utils::size(map), "out of range enum");
    return printArgs(ctx, map[statusIdx]);
}

template<typename K, typename V>
inline ssize
formatToContext(Context ctx, FormatArgs, const MapBucket<K, V>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.key, x.val);
}

template<typename K, typename V>
inline ssize
formatToContext(Context ctx, FormatArgs, const KeyVal<K, V>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.key, x.val);
}

} /* namespace print */

} /* namespace adt */

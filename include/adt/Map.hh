/* Hashmap with linear probing.
 * For custom hash function add template<> hash::func(const KeyType& x), (or specify in the template argument)
 * and bool operator==(const KeyType& other) */

#pragma once

#include "Vec.hh"
#include "defer.hh"
#include "hash.hh"
#include "print.hh"

namespace adt
{

constexpr f32 MAP_DEFAULT_LOAD_FACTOR = 0.5f;
constexpr f32 MAP_DEFAULT_LOAD_FACTOR_INV = 1.0f / MAP_DEFAULT_LOAD_FACTOR;

enum class MAP_RESULT_STATUS : u8 { NOT_FOUND, FOUND, INSERTED };

enum class MAP_BUCKET_FLAGS : u8
{
    NONE = 0,
    OCCUPIED = 1,
    DELETED = 2,
};

template<typename K, typename V>
struct KeyVal
{
    K key {};
    ADT_NO_UNIQUE_ADDRESS V val {}; /* ADT_NO_UNIQUE_ADDRESS for empty values */
};

template<typename K, typename V>
struct MapBucket
{
    K key {};
    ADT_NO_UNIQUE_ADDRESS V val {}; /* ADT_NO_UNIQUE_ADDRESS for empty values */
    MAP_BUCKET_FLAGS eFlags {};
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
    valueOr(const V& v) const
    {
        return valueOrEmplace(v);
    }

    [[nodiscard]] const V
    valueOr(V&& v) const
    {
        return valueOrEmplace(std::move(v));
    }

    template<typename ...ARGS>
    [[nodiscard]] const V
    valueOrEmplace(ARGS&&... v) const
    {
        if (eStatus != MAP_RESULT_STATUS::NOT_FOUND)
            return value();
        else return V (std::forward<ARGS>(v)...);
    }
};

template<typename K, typename V, usize (*FN_HASH)(const K&) = hash::func<K>>
struct Map
{
    Vec<MapBucket<K, V>> m_vBuckets {};
    isize m_nOccupied {};
    f32 m_maxLoadFactor {};

    /* */

    Map() = default;
    Map(IAllocator* pAllocator, isize prealloc = SIZE_MIN, f32 loadFactor = MAP_DEFAULT_LOAD_FACTOR);

    /* */

    auto& data() { return m_vBuckets.data(); }
    const auto& data() const { return m_vBuckets.data(); }

    [[nodiscard]] bool empty() const { return m_nOccupied <= 0; }

    [[nodiscard]] isize idx(const KeyVal<K, V>* p) const;

    [[nodiscard]] isize idx(const MapResult<K, V> res) const;

    [[nodiscard]] isize firstI() const;

    [[nodiscard]] isize nextI(isize i) const;

    [[nodiscard]] f32 loadFactor() const;

    MapResult<K, V> insert(IAllocator* p, const K& key, const V& val);
    MapResult<K, V> insert(IAllocator* p, const K& key, V&& val);

    template<typename ...ARGS> requires(std::is_constructible_v<V, ARGS...>)
    MapResult<K, V> emplace(IAllocator* p, const K& key, ARGS&&... args);

    template<typename ...ARGS> requires(std::is_constructible_v<V, ARGS...>)
    MapResult<K, V> emplaceHashed(IAllocator* p, const K& key, const usize keyHash, ARGS&&... args);

    [[nodiscard]] MapResult<K, V> search(const K& key);
    [[nodiscard]] const MapResult<K, V> search(const K& key) const;

    void remove(isize i);

    void remove(const K& key);

    bool tryRemove(const K& key);

    MapResult<K, V> tryInsert(IAllocator* p, const K& key, const V& val);
    MapResult<K, V> tryInsert(IAllocator* p, const K& key, V&& val);

    template<typename ...ARGS>
    MapResult<K, V> tryEmplace(IAllocator* p, const K& key, ARGS&&... args);

    void destroy(IAllocator* p) noexcept;

    [[nodiscard]] Map release() noexcept;

    [[nodiscard]] isize cap() const;

    [[nodiscard]] isize size() const;

    void rehash(IAllocator* p, isize size);

    [[nodiscard]] MapResult<K, V> searchHashed(const K& key, usize keyHash) const;

    void zeroOut();

    isize insertionIdx(usize hash, const K& key) const;

    /* */

public:
    struct It
    {
        Map* s {};
        isize i = 0;

        It(Map* _s, isize _i) : s(_s), i(_i) {}

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

protected:
#if !defined NDEBUG && defined ADT_DBG_COLLISIONS
    mutable i64 m_nCollisions = 0;
#endif
};

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline isize
Map<K, V, FN_HASH>::idx(const KeyVal<K, V>* p) const
{
    isize r = (MapBucket<K, V>*)p - &m_vBuckets[0];
    ADT_ASSERT(r >= 0 && r < m_vBuckets.cap(), "out of range, r: {}, cap: {}", r, m_vBuckets.cap());
    return r;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline isize
Map<K, V, FN_HASH>::idx(const MapResult<K, V> res) const
{
    isize idx = res.pData - &m_vBuckets[0];
    ADT_ASSERT(idx >= 0 && idx < m_vBuckets.cap(), "out of range, r: {}, cap: {}", idx, m_vBuckets.cap());
    return idx;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline isize
Map<K, V, FN_HASH>::firstI() const
{
    isize i = 0;
    while (i < m_vBuckets.cap() && m_vBuckets[i].eFlags != MAP_BUCKET_FLAGS::OCCUPIED)
        ++i;

    if (i >= m_vBuckets.cap()) i = NPOS;

    return i;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline isize
Map<K, V, FN_HASH>::nextI(isize i) const
{
    do ++i;
    while (i < m_vBuckets.cap() && m_vBuckets[i].eFlags != MAP_BUCKET_FLAGS::OCCUPIED);

    if (i >= m_vBuckets.cap()) i = NPOS;

    return i;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline f32
Map<K, V, FN_HASH>::loadFactor() const
{
    ADT_ASSERT(m_vBuckets.cap() > 0, "cap: {}", m_vBuckets.cap());
    return f32(m_nOccupied) / f32(m_vBuckets.cap());
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline MapResult<K, V>
Map<K, V, FN_HASH>::insert(IAllocator* p, const K& key, const V& val)
{
    return emplace(p, key, val);
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline MapResult<K, V>
Map<K, V, FN_HASH>::insert(IAllocator* p, const K& key, V&& val)
{
    return emplace(p, key, std::move(val));
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
template<typename ...ARGS> requires(std::is_constructible_v<V, ARGS...>)
inline MapResult<K, V>
Map<K, V, FN_HASH>::emplace(IAllocator* p, const K& key, ARGS&&... args)
{
    return emplaceHashed(p, key, FN_HASH(key), std::forward<ARGS>(args)...);
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
template<typename ...ARGS> requires(std::is_constructible_v<V, ARGS...>)
inline MapResult<K, V>
Map<K, V, FN_HASH>::emplaceHashed(IAllocator* p, const K& key, const usize keyHash, ARGS&&... args)
{
    if (m_vBuckets.cap() <= 0)
        *this = {p};
    else if (loadFactor() >= m_maxLoadFactor)
        rehash(p, m_vBuckets.cap() * 2);

    const isize idx = insertionIdx(keyHash, key);
    auto& bucket = m_vBuckets[idx];

    V tmpVal = V (std::forward<ARGS>(args)...);

    ADT_DEFER(
        utils::moveDestruct(&bucket.val, std::move(tmpVal));
        bucket.eFlags = MAP_BUCKET_FLAGS::OCCUPIED;
    );

    if (bucket.key == key)
    {
#ifndef NDEBUG
        print::err("[Map::emplace]: updating value for existing key('{}'): old: '{}', new: '{}'\n",
            key, bucket.val, tmpVal
        );
#endif

        return {
            .pData = &bucket,
            .hash = keyHash,
            .eStatus = MAP_RESULT_STATUS::FOUND,
        };
    }

    if constexpr (!std::is_trivially_destructible_v<K>) bucket.key.~K();
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
Map<K, V, FN_HASH>::search(const K& key)
{
    return searchHashed(key, FN_HASH(key));
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
[[nodiscard]] inline const MapResult<K, V>
Map<K, V, FN_HASH>::search(const K& key) const
{
    return searchHashed(key, FN_HASH(key));
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline void
Map<K, V, FN_HASH>::remove(isize i)
{
    auto& bucket = m_vBuckets[i];

    if constexpr (!std::is_trivially_destructible_v<K>)
        bucket.key.~K();
    new(&bucket.key) K {};

    if constexpr (!std::is_trivially_destructible_v<V>)
        bucket.val.~V();
    new(&bucket.val) V {};

    bucket.eFlags = MAP_BUCKET_FLAGS::DELETED;

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
    return tryEmplace(p, key, val);
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline MapResult<K, V>
Map<K, V, FN_HASH>::tryInsert(IAllocator* p, const K& key, V&& val)
{
    return tryEmplace(p, key, std::move(val));
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
template<typename ...ARGS>
inline MapResult<K, V>
Map<K, V, FN_HASH>::tryEmplace(IAllocator* p, const K& key, ARGS&&... args)
{
    const usize keyHash = FN_HASH(key);
    auto f = searchHashed(key, keyHash);
    if (f) return f;
    else return emplaceHashed(p, key, keyHash, std::forward<ARGS>(args)...);
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline void
Map<K, V, FN_HASH>::destroy(IAllocator* p) noexcept
{
    m_vBuckets.destroy(p);
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline Map<K, V, FN_HASH>
Map<K, V, FN_HASH>::release() noexcept
{
    return utils::exchange(this, {});
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline isize Map<K, V, FN_HASH>::cap() const
{
    return m_vBuckets.cap();
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline isize Map<K, V, FN_HASH>::size() const
{
    return m_nOccupied;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
inline void
Map<K, V, FN_HASH>::rehash(IAllocator* p, isize size)
{
    ADT_ASSERT(isPowerOf2(size), "size: {}", size);

    Map mNew = Map(p, size);

    for (const auto& [key, val] : *this)
        mNew.emplace(p, key, val);

    destroy(p);
    *this = mNew;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
[[nodiscard]] inline MapResult<K, V>
Map<K, V, FN_HASH>::searchHashed(const K& key, usize keyHash) const
{
    MapResult<K, V> res {.eStatus = MAP_RESULT_STATUS::NOT_FOUND};

    if (m_nOccupied == 0)
    {
#ifndef NDEBUG
        print::err("[Map::search]: m_nOccupied: {}\n", m_nOccupied);
#endif
        return res;
    }

    isize idx = isize(keyHash & usize(m_vBuckets.cap() - 1));
    res.hash = keyHash;

    while (m_vBuckets[idx].eFlags != MAP_BUCKET_FLAGS::NONE) /* deleted or occupied */
    {
        if (m_vBuckets[idx].eFlags != MAP_BUCKET_FLAGS::DELETED &&
            m_vBuckets[idx].key == key
        )
        {
            res.pData = const_cast<MapBucket<K, V>*>(&m_vBuckets[idx]);
            res.eStatus = MAP_RESULT_STATUS::FOUND;
            break;
        }

        idx = utils::cycleForwardPowerOf2(idx, m_vBuckets.size());
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
inline isize
Map<K, V, FN_HASH>::insertionIdx(usize hash, const K& key) const
{
    isize idx = isize(hash & usize(m_vBuckets.cap() - 1));

    while (m_vBuckets[idx].eFlags == MAP_BUCKET_FLAGS::OCCUPIED)
    {
        if (m_vBuckets[idx].key == key) break;

#if !defined NDEBUG && defined ADT_DBG_COLLISIONS
        print::err("[Map::insertionIdx]: collision at: {} (keys: '{}' and '{}'), nCollisions: {}\n", idx, key, m_vBuckets[idx].key, m_nCollisions++);
#endif
        idx = utils::cycleForwardPowerOf2(idx, m_vBuckets.size());
    }

    return idx;
}

template<typename K, typename V, usize (*FN_HASH)(const K&)>
Map<K, V, FN_HASH>::Map(IAllocator* pAllocator, isize prealloc, f32 loadFactor)
    : m_vBuckets {pAllocator, nextPowerOf2(isize(prealloc / loadFactor))},
      m_nOccupied {},
      m_maxLoadFactor {loadFactor}
{
    ADT_ASSERT(isPowerOf2(m_vBuckets.cap()), "");
    m_vBuckets.setSize(pAllocator, m_vBuckets.cap());
}

template<typename K, typename V, typename ALLOC_T = StdAllocatorNV, usize (*FN_HASH)(const K&) = hash::func<K>>
struct MapManaged : public Map<K, V, FN_HASH>
{
    using Base = Map<K, V, FN_HASH>;

    /* */

    ADT_NO_UNIQUE_ADDRESS ALLOC_T m_alloc;
    /* */

    MapManaged() = default;
    MapManaged(isize prealloc, f32 loadFactor = MAP_DEFAULT_LOAD_FACTOR)
        : Base::Map(&allocator(), prealloc, loadFactor) {}

    /* */

    ALLOC_T& allocator() { return m_alloc; }
    const ALLOC_T& allocator() const { return m_alloc; }

    MapResult<K, V> insert(const K& key, const V& val) { return Base::insert(&allocator(), key, val); }
    MapResult<K, V> insert(const K& key, V&& val) { return Base::insert(&allocator(), key, std::move(val)); }

    template<typename ...ARGS> requires(std::is_constructible_v<V, ARGS...>) MapResult<K, V> emplace(const K& key, ARGS&&... args)
    { return Base::emplace(&allocator(), key, std::forward<ARGS>(args)...); }

    MapResult<K, V> tryInsert(const K& key, const V& val) { return Base::tryInsert(&allocator(), key, val); }
    MapResult<K, V> tryInsert(const K& key, V&& val) { return Base::tryInsert(&allocator(), key, std::move(val)); }

    template<typename ...ARGS>
    MapResult<K, V> tryEmplace(const K& key, ARGS&&... args)
    { return Base::tryEmplace(&allocator(), key, std::forward<ARGS>(args)...); }

    void destroy() noexcept { Base::destroy(&allocator()); }

    MapManaged release() noexcept { return utils::exchange(this, {}); }
};

template<typename K, typename V>
using MapM = MapManaged<K, V>;

namespace print
{

inline isize
formatToContext(Context ctx, FormatArgs, const MAP_RESULT_STATUS eStatus)
{
    ctx.fmt = "{}";
    ctx.fmtIdx = 0;
    constexpr StringView map[] {
        "NOT_FOUND", "FOUND", "INSERTED"
    };

    auto statusIdx = std::underlying_type_t<MAP_RESULT_STATUS>(eStatus);
    ADT_ASSERT(statusIdx >= 0 && statusIdx < utils::size(map), "out of range enum");
    return printArgs(ctx, map[statusIdx]);
}

template<typename K, typename V>
inline isize
formatToContext(Context ctx, FormatArgs, const MapBucket<K, V>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.key, x.val);
}

template<typename K, typename V>
inline isize
formatToContext(Context ctx, FormatArgs, const KeyVal<K, V>& x)
{
    ctx.fmt = "[{}, {}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.key, x.val);
}

template<typename K, typename V>
inline isize
formatToContext(Context ctx, FormatArgs, const MapResult<K, V>& x)
{
    ctx.fmt = "{}, {}, {}";
    ctx.fmtIdx = 0;

    if (x.pData) return printArgs(ctx, x.data(), x.hash, x.eStatus);
    return printArgs(ctx, x.pData, x.hash, x.eStatus);
}

} /* namespace print */

} /* namespace adt */

#pragma once

#include "Map.hh"

namespace adt
{

template<typename T>
struct SetResult : public MapResult<T, Empty>
{
    using Base = MapResult<T, Empty>;

    /* */

    SetResult() = default;

    SetResult(const Base& res) : Base::MapResult(res) {}

    SetResult(MapBucket<T, Empty>* _pBucket, usize _hash, MAP_RESULT_STATUS _eStatus)
        : Base::MapResult {_pBucket, _hash, _eStatus} {}

    /* */

    T&
    data()
    {
        ADT_ASSERT(Base::eStatus != MAP_RESULT_STATUS::NOT_FOUND, "not found");
        return *(T*)Base::pData;
    }

    const T&
    data() const
    {
        ADT_ASSERT(Base::eStatus != MAP_RESULT_STATUS::NOT_FOUND, "not found");
        return *(T*)Base::pData;
    }

    T& value() { return data(); }
    const T& value() const { return data(); }

    auto key() = delete;

    [[nodiscard]] const T
    valueOr(T&& v) const
    {
        if (Base::eStatus != MAP_RESULT_STATUS::NOT_FOUND)
            return value();
        else return std::forward<T>(v);
    }
};

/* Like Map, but keys are also values */
template<typename T, usize (*FN_HASH)(const T&) = hash::func<T>>
struct Set : public Map<T, Empty, FN_HASH>
{
    using Base = Map<T, Empty, FN_HASH>;

    /* */

    using Base::Base;

    /* */

    SetResult<T> insert(IAllocator* p, const T& x);
    auto tryInsert() = delete;
    SetResult<T> insertHashed(const T& x, usize hash);

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    SetResult<T> emplace(IAllocator* p, ARGS&&... args) { return insert(p, {std::forward<T>(args)...}); }

    [[nodiscard]] SetResult<T> search(const T& key);
    [[nodiscard]] const SetResult<T> search(const T& key) const;

    [[nodiscard]] SetResult<T> searchHashed(const T& key, usize keyHash) const { return Base::searchHashed(key, keyHash); }

    void rehash(IAllocator* p, isize size);

    isize idx(const T* const p) const;
    isize idx(const SetResult<T> res) const;

    /* */

    struct It
    {
        Set* s {};
        isize i = 0;

        It(Set* _s, isize _i) : s(_s), i(_i) {}

        T& operator*() { return *(T*)&s->m_vBuckets[i]; }
        T* operator->() { return (T*)&s->m_vBuckets[i]; }

        It operator++()
        {
            i = s->nextI(i);
            return {s, i};
        }
        It operator++(int) { auto* tmp = s++; return tmp; }

        friend bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend bool operator!=(const It& l, const It& r) { return l.i != r.i; }
    };

    It begin() { return {this, Base::firstI()}; }
    It end() { return {this, NPOS}; }

    const It begin() const { return {this, Base::firstI()}; }
    const It end() const { return {this, NPOS}; }
};

template<typename T, usize (*FN_HASH)(const T&)>
inline SetResult<T>
Set<T, FN_HASH>::insert(IAllocator* p, const T& x)
{
    if (Base::m_vBuckets.cap() <= 0)
        *this = {p};
    else if (Base::loadFactor() >= Base::m_maxLoadFactor)
        rehash(p, Base::m_vBuckets.cap() * 2);

    return insertHashed(x, FN_HASH(x));
}

template<typename T, usize (*FN_HASH)(const T&)>
inline SetResult<T>
Set<T, FN_HASH>::insertHashed(const T& x, const usize hash)
{
    const isize idx = Base::insertionIdx(hash, x);
    auto& rBucket = Base::m_vBuckets[idx];

    rBucket.eFlags = MAP_BUCKET_FLAGS::OCCUPIED;

    SetResult<T> res {
        &rBucket, hash, MAP_RESULT_STATUS::INSERTED
    };

    if (rBucket.key == x)
    {
        res.eStatus = MAP_RESULT_STATUS::FOUND;
        return res;
    }

    new(&rBucket.key) T(x);
    ++Base::m_nOccupied;

    return res;
}

template<typename T, usize (*FN_HASH)(const T&)>
inline SetResult<T>
Set<T, FN_HASH>::search(const T& key)
{
    return Base::searchHashed(key, FN_HASH(key));
}

template<typename T, usize (*FN_HASH)(const T&)>
inline const SetResult<T>
Set<T, FN_HASH>::search(const T& key) const
{
    return Base::searchHashed(key, FN_HASH(key));
}

template<typename T, usize (*FN_HASH)(const T&)>
inline void
Set<T, FN_HASH>::rehash(IAllocator* p, isize size)
{
    Set mNew {p, size};

    for (const auto& key : *this)
        mNew.insert(p, key);

    Base::destroy(p);
    *this = mNew;
}

template<typename T, usize (*FN_HASH)(const T&)>
inline isize
Set<T, FN_HASH>::idx(const T* const p) const
{
    const isize r = (MapBucket<T, Empty>*)p - &Base::m_vBuckets[0];
    ADT_ASSERT(r >= 0 && r < Base::m_vBuckets.cap(), "out of range, r: {}, cap: {}", r, Base::m_vBuckets.cap());
    return r;
}

template<typename T, usize (*FN_HASH)(const T&)>
inline isize
Set<T, FN_HASH>::idx(const SetResult<T> res) const
{
    isize idx = res.pData - &Base::m_vBuckets[0];
    ADT_ASSERT(idx >= 0 && idx < Base::m_vBuckets.cap(), "out of range, r: {}, cap: {}", idx, Base::m_vBuckets.cap());
    return idx;
}

} /* namespace adt */

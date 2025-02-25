#pragma once

#include "hash.hh"
#include "Vec.hh"

namespace adt
{

constexpr f32 SET_DEFAULT_LOAD_FACTOR = 0.5f;
constexpr f32 SET_DEFAULT_LOAD_FACTOR_INV = 1.0f / SET_DEFAULT_LOAD_FACTOR;

template<typename T>
struct SetBucket
{
    T val {};
    bool bOccupied = false;
    bool bDeleted = false;
};

/* custom return type for insert/search operations */
template<typename T>
struct SetResult
{
    enum class STATUS : u8 { FOUND, NOT_FOUND, INSERTED };

    /* */

    SetBucket<T> m_val {};
    usize m_hash {};
    STATUS m_eStatus {};

    explicit constexpr operator bool() const
    {
        return m_eStatus != STATUS::NOT_FOUND;
    }

    constexpr const T&
    value() const
    {
        return m_val.val;
    }

    constexpr const T&
    valueOr(T&& _or) const
    {
        if (operator bool())
            return value();
        else return std::forward<T>(_or);
    }
};

/* Like hash map but key is also a value. */
template<typename T, usize (*FN_HASH)(const T&) = hash::func<T>>
struct Set
{
    Vec<SetBucket<T>> m_vBuckets {};

    /* */

    Set() = default;
    Set(IAllocator* pAllocator, ssize prealloc = SIZE_MIN);

    /* */
};

} /* namespace adt */

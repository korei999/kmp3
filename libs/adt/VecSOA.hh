#pragma once

#include "IAllocator.hh"
#include "utils.hh"

namespace adt
{

/* NOTE: experimental.
 * This template allows to create arbitrary vectors with SOA memory layout.
 * Creation example:
 * 
 * // SOA.hh can generate these
 * struct Entity
 * {
 *     // reference mirror
 *     struct Bind
 *     {
 *         math::V3& pos;
 *         math::V3& vel;
 *         int& index;
 *     }
 *     
 *     math::V3 pos {};
 *     math::V3 vel {};
 *     int index {};
 * };
 * 
 * Must preserve the order.
 * VecSOA<Entity, Entity::Bind, &Entity::pos, &Entity::vel, &Entity::index> vec(StdAllocator::inst());
 *
 * Works with regular vector syntax:
 * for (auto bind : vec) bind.vel = {};
 * vec[3].index = 2;
 * 
 * Very ugly, but gets the job done. */
template<typename STRUCT, typename BIND, auto ...MEMBERS>
struct VecSOA
{
    u8* m_pData {};
    ssize m_size {};
    ssize m_capacity {};

    /* */

    VecSOA() = default;
    VecSOA(IAllocator* pAlloc, ssize prealloc = SIZE_MIN);

    /* */

    BIND operator[](const ssize i) { return bind(i); }
    const BIND operator[](const ssize i) const { return bind(i); }

    BIND first() { return operator[](0); }
    const BIND first() const { return operator[](0); }
    BIND last() { return operator[](m_size - 1); }
    const BIND last() const { return operator[](m_size - 1); }

    void destroy(IAllocator* pAlloc);
    ssize push(IAllocator* pAlloc, const STRUCT& x);
    void pop() { --m_size; }
    ssize size() const { return m_size; }
    ssize cap() const { return m_capacity; }
    void setSize(IAllocator* pAlloc, const ssize newSize);
    void setCap(IAllocator* pAlloc, const ssize newCap);
    void zeroOut();
    ssize totalByteCap() const;

protected:
    BIND bind(const ssize i) const;
    void set(ssize i, const STRUCT& x);
    void growIfNeeded(IAllocator* pAlloc);
    void grow(IAllocator* p, ssize newCapacity);

public:

    struct It
    {
        VecSOA* s;
        ssize i {};

        It(const VecSOA* _s, ssize _i) : s(const_cast<VecSOA*>(_s)), i(_i) {}

        auto operator*() noexcept { return s->operator[](i); }

        It operator++() noexcept { ++i; return *this; }
        It operator--() noexcept { --i; return *this; }

        friend constexpr bool operator==(const It& l, const It& r) noexcept { return l.i == r.i; }
        friend constexpr bool operator!=(const It& l, const It& r) noexcept { return l.i != r.i; }
    };

    It begin() noexcept { return {this, 0}; }
    It end() noexcept { return {this, size()}; }
    It rbegin() noexcept { return {this, size() - 1}; }
    It rend() noexcept { return {this, -1}; }

    const It begin() const noexcept { return {this, 0}; }
    const It end() const noexcept { return {this, size()}; }
    const It rbegin() const noexcept { return {this, size() - 1}; }
    const It rend() const noexcept { return {this, -1}; }
};

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline
VecSOA<STRUCT, BIND, MEMBERS...>::VecSOA(IAllocator* pAlloc, ssize prealloc)
    : m_pData(pAlloc->mallocV<u8>(prealloc * sizeof(STRUCT))), m_capacity(prealloc) {}

template<typename STRUCT, typename BIND>
inline void VecSOASetHelper(u8*, const ssize, const ssize) { /* empty case */ }

template<typename STRUCT, typename BIND, typename HEAD, typename ...TAIL>
inline void
VecSOASetHelper(u8* pData, const ssize cap, const ssize i, HEAD&& head, TAIL&&... tail)
{
    using HeadType = std::remove_reference_t<HEAD>;

    auto* pPlacement = reinterpret_cast<HeadType*>(pData) + i;
    new( (void*)(pPlacement) ) HeadType(head);

    VecSOASetHelper<STRUCT, BIND>(
        (u8*)((reinterpret_cast<HeadType*>(pData)) + cap), cap, i, std::forward<TAIL>(tail)...
    );
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::set(ssize i, const STRUCT& x)
{
    ADT_ASSERT(i >= 0 && i < m_size, "out of range: i: {}, size: {}\n", i, m_size);
    VecSOASetHelper<STRUCT, BIND>(m_pData, m_capacity, i, x.*MEMBERS...);
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline ssize
VecSOA<STRUCT, BIND, MEMBERS...>::push(IAllocator* pAlloc, const STRUCT& x)
{
    growIfNeeded(pAlloc);
    ++m_size;
    set(m_size - 1, x);
    return m_size - 1;
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::setSize(IAllocator* pAlloc, const ssize newSize)
{
    if (m_capacity < newSize) grow(pAlloc, newSize);
    m_size = newSize;
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::setCap(IAllocator* pAlloc, const ssize newCap)
{
    if (newCap == 0)
    {
        destroy(pAlloc);
        return;
    }

    m_pData = (u8*)pAlloc->realloc(m_pData, m_capacity, newCap, sizeof(STRUCT));
    m_capacity = newCap;

    if (m_size > newCap) m_size = newCap;
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::zeroOut()
{
    memset(m_pData, 0, totalByteCap());
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline ssize
VecSOA<STRUCT, BIND, MEMBERS...>::totalByteCap() const
{
    return cap() * sizeof(STRUCT);
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::growIfNeeded(IAllocator* pAlloc)
{
    if (m_size < m_capacity) return;

    ssize newCap = utils::max(decltype(m_capacity)(SIZE_MIN), m_capacity*2);
    ADT_ASSERT(newCap > m_capacity, "can't grow (capacity overflow), newCap: {}, m_capacity: {}", newCap, m_capacity);
    grow(pAlloc, newCap);
}

template<typename STRUCT, typename BIND>
inline void VecSOAReallocHelper(const u8* const, u8*, const ssize, const ssize) { /* empty case */ }

template<typename STRUCT, typename BIND, typename HEAD, typename ...TAIL>
inline void
VecSOAReallocHelper(const u8* const pOld, u8* pData, const ssize oldCap, const ssize newCap)
{
    using HeadType = std::remove_reference_t<HEAD>;

    memcpy(pData, pOld, oldCap * sizeof(HeadType));

    u8* pNextOld = (u8*)(reinterpret_cast<const HeadType*>(pOld) + oldCap);
    u8* pNextData = (u8*)(reinterpret_cast<const HeadType*>(pData) + newCap);

    VecSOAReallocHelper<STRUCT, BIND, TAIL...>(
        pNextOld, pNextData, oldCap, newCap
    );
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::grow(IAllocator* p, ssize newCapacity)
{
    u8* pNewData = p->mallocV<u8>(newCapacity * sizeof(STRUCT));

    VecSOAReallocHelper<STRUCT, BIND, decltype(std::declval<STRUCT>().*MEMBERS)...>(
        m_pData, pNewData, m_capacity, newCapacity
    );

    m_capacity = newCapacity;
    p->free(m_pData);
    m_pData = pNewData;
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::destroy(IAllocator* pAlloc)
{
    pAlloc->free(m_pData);
    *this = {};
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline BIND
VecSOA<STRUCT, BIND, MEMBERS...>::bind(const ssize i) const
{
    ADT_ASSERT(i >= 0 && i < m_size, "out of range: i: {}, size: {}\n", i, m_size);

    u8* p = m_pData;
    ssize off = 0;

    return {
        (
            [&]() -> auto&
            {
                using FieldType = std::remove_reference_t<decltype(std::declval<STRUCT>().*MEMBERS)>;

                auto& ref = reinterpret_cast<FieldType*>(p + off)[i];

                off += cap() * sizeof(FieldType);
                return ref;
            }()
        )...
    };
}

} /* namespace adt */

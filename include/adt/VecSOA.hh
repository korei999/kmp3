#pragma once

#include "IAllocator.hh"
#include "utils.hh"

namespace adt
{

/* NOTE: experimental.
 * This template allows to create arbitrary vectors with SOA memory layout.
 * Creation example:
 * 
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
 * Using SOA.hh:
 * #define ENTITY_PP_BIND_I(TYPE, NAME) , &Entity::NAME
 * #define ENTITY_PP_BIND(TUPLE) ENTITY_PP_BIND_I TUPLE
 * #define ENTITY_FIELDS \
 *     (adt::StringFixed<128>, sfName),
 *     (adt::math::V4, color),
 *     (adt::math::V3, pos),
 *     (adt::math::Qt, rot),
 *     (adt::math::V3, scale),
 *     (adt::math::V3, vel),
 *     (adt::i16, assetI),
 *     (adt::i16, modelI),
 *     (bool, bNoDraw)
 * ADT_SOA_GEN_STRUCT_ZERO(Entity, Bind, ENTITY_FIELDS);
 * #define ENTITY_TEMPLATE_ARGS Entity, Entity::Bind ADT_PP_FOR_EACH(ENTITY_PP_BIND, ENTITY_FIELDS)
 *
 * VecSOA<ENTITY_TEMPLATE_ARGS> v0 {&alloc, SIZE};
 *
 * Works with regular vector syntax:
 * for (auto bind : vec) bind.vel = {};
 * vec[3].index = 2; */

template<typename STRUCT, typename BIND, auto ...MEMBERS>
struct VecSOA
{
    u8* m_pData {};
    isize m_size {};
    isize m_capacity {};

    /* */

    VecSOA() = default;
    VecSOA(IAllocator* pAlloc, isize prealloc = SIZE_MIN);

    /* */

    BIND operator[](const isize i) noexcept { return bind(i); }
    const BIND operator[](const isize i) const noexcept { return bind(i); }

    BIND first() noexcept { return operator[](0); }
    const BIND first() const noexcept { return operator[](0); }
    BIND last() noexcept { return operator[](m_size - 1); }
    const BIND last() const noexcept { return operator[](m_size - 1); }

    void destroy(IAllocator* pAlloc) noexcept;
    isize push(IAllocator* pAlloc, const STRUCT& x);
    void pop() noexcept { --m_size; }
    isize size() const noexcept { return m_size; }
    isize cap() const noexcept { return m_capacity; }
    void setSize(IAllocator* pAlloc, const isize newSize);
    void zeroOut() noexcept;
    isize totalByteCap() const noexcept;

protected:
    BIND bind(const isize i) const noexcept;
    void set(isize i, const STRUCT& x);
    void growIfNeeded(IAllocator* pAlloc);
    void grow(IAllocator* p, isize newCapacity);

public:

    struct It
    {
        VecSOA* s;
        isize i {};

        It(const VecSOA* _s, isize _i) : s(const_cast<VecSOA*>(_s)), i(_i) {}

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
VecSOA<STRUCT, BIND, MEMBERS...>::VecSOA(IAllocator* pAlloc, isize prealloc)
    : m_pData {pAlloc->zallocV<u8>(prealloc * sizeof(STRUCT))}, m_capacity(prealloc) {}

namespace details
{

template<typename STRUCT, typename BIND>
inline void VecSOASetHelper(u8*, const isize, const isize) noexcept { /* empty case */ }

template<typename STRUCT, typename BIND, typename HEAD, typename ...TAIL>
inline void
VecSOASetHelper(u8* pData, const isize cap, const isize i, HEAD&& head, TAIL&&... tail) noexcept
{
    using HeadType = std::remove_reference_t<HEAD>;

    auto* pPlacement = reinterpret_cast<HeadType*>(pData) + i;
    new( (void*)(pPlacement) ) HeadType(head);

    details::VecSOASetHelper<STRUCT, BIND>(
        (u8*)((reinterpret_cast<HeadType*>(pData)) + cap), cap, i, std::forward<TAIL>(tail)...
    );
}

} /* namespace details */

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::set(isize i, const STRUCT& x)
{
    ADT_ASSERT(i >= 0 && i < m_size, "out of range: i: {}, size: {}\n", i, m_size);
    details::VecSOASetHelper<STRUCT, BIND>(m_pData, m_capacity, i, x.*MEMBERS...);
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline isize
VecSOA<STRUCT, BIND, MEMBERS...>::push(IAllocator* pAlloc, const STRUCT& x)
{
    growIfNeeded(pAlloc);
    ++m_size;
    set(m_size - 1, x);
    return m_size - 1;
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::setSize(IAllocator* pAlloc, const isize newSize)
{
    if (m_capacity < newSize) grow(pAlloc, newSize);
    m_size = newSize;
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::zeroOut() noexcept
{
    memset(m_pData, 0, totalByteCap());
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline isize
VecSOA<STRUCT, BIND, MEMBERS...>::totalByteCap() const noexcept
{
    return cap() * sizeof(STRUCT);
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::growIfNeeded(IAllocator* pAlloc)
{
    if (m_size < m_capacity) return;

    isize newCap = utils::max(decltype(m_capacity)(SIZE_MIN), m_capacity*2);
    ADT_ASSERT(newCap > m_capacity, "can't grow (capacity overflow), newCap: {}, m_capacity: {}", newCap, m_capacity);
    grow(pAlloc, newCap);
}

template<typename STRUCT, typename BIND>
inline void VecSOAReallocHelper(const u8* const, u8*, const isize, const isize) { /* empty case */ }

template<typename STRUCT, typename BIND, typename HEAD, typename ...TAIL>
inline void
VecSOAReallocHelper(const u8* const pOld, u8* pData, const isize oldCap, const isize newCap)
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
VecSOA<STRUCT, BIND, MEMBERS...>::grow(IAllocator* p, isize newCapacity)
{
    u8* pNewData = p->zallocV<u8>(newCapacity * sizeof(STRUCT));

    VecSOAReallocHelper<STRUCT, BIND, decltype(std::declval<STRUCT>().*MEMBERS)...>(
        m_pData, pNewData, m_capacity, newCapacity
    );

    m_capacity = newCapacity;
    p->free(m_pData);
    m_pData = pNewData;
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline void
VecSOA<STRUCT, BIND, MEMBERS...>::destroy(IAllocator* pAlloc) noexcept
{
    pAlloc->dealloc(m_pData, m_size);
    *this = {};
}

template<typename STRUCT, typename BIND, auto ...MEMBERS>
inline BIND
VecSOA<STRUCT, BIND, MEMBERS...>::bind(const isize i) const noexcept
{
    ADT_ASSERT(i >= 0 && i < m_size, "out of range: i: {}, size: {}\n", i, m_size);

    u8* p = m_pData;
    isize off = 0;

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

template<typename ALLOC_T, typename STRUCT, typename BIND, auto ...MEMBERS>
struct VecSOAManaged : VecSOA<STRUCT, BIND, MEMBERS...>
{
    using Base = VecSOA<STRUCT, BIND, MEMBERS...>;

    /* */

    ADT_NO_UNIQUE_ADDRESS ALLOC_T m_alloc;

    /* */

    VecSOAManaged() = default;
    VecSOAManaged(const isize prealloc) : Base {&allocator(), prealloc} {}

    /* */

    ALLOC_T& allocator() { return m_alloc; }
    const ALLOC_T& allocator() const { return m_alloc; }

    void destroy() noexcept { Base::destroy(&allocator()); }
    isize push(const STRUCT& x) { return Base::push(&allocator(), x); }
    void setSize(const isize newSize) { Base::setSize(&allocator(), newSize); }
};

template<typename STRUCT, typename BIND, auto ...MEMBERS>
using VecSOAM = VecSOAManaged<StdAllocatorNV, STRUCT, BIND, MEMBERS...>;

} /* namespace adt */

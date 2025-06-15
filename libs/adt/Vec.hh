#pragma once

#include "StdAllocator.hh"
#include "sort.hh"

namespace adt
{

/* TODO: overflow checks are pretty naive */

/* Dynamic array (aka Vector) */
template<typename T>
struct Vec
{
    T* m_pData = nullptr;
    isize m_size = 0;
    isize m_capacity = 0;

    /* */

    Vec() = default;

    Vec(IAllocator* p, isize prealloc = SIZE_MIN)
        : m_pData((T*)p->zalloc(prealloc, sizeof(T))),
          m_size(0),
          m_capacity(prealloc) {}

    Vec(IAllocator* p, isize preallocSize, const T& fillWith);

    /* */

    explicit operator Span<T>() { return {m_pData, m_size}; }
    explicit operator const Span<const T>() const { return {m_pData, m_size}; }

    /* */

#define ADT_RANGE_CHECK ADT_ASSERT(i >= 0 && i < m_size, "i: {}, m_size: {}", i, m_size);

    [[nodiscard]] T& operator[](isize i)             noexcept { ADT_RANGE_CHECK; return m_pData[i]; }
    [[nodiscard]] const T& operator[](isize i) const noexcept { ADT_RANGE_CHECK; return m_pData[i]; }

#undef ADT_RANGE_CHECK

    [[nodiscard]] bool empty() const noexcept { return m_size <= 0; }

    isize fakePush(IAllocator* p);

    isize push(IAllocator* p, const T& data);

    void pushAt(IAllocator* p, const isize atI, const T& data);

    isize pushSpan(IAllocator* p, const Span<const T> sp);

    void pushSpanAt(IAllocator* p, const isize atI, const Span<const T> sp);

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    isize emplace(IAllocator* p, ARGS&&... args);

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    void emplaceAt(IAllocator* p, const isize atI, ARGS&&... args);

    isize pushSorted(IAllocator* p, const sort::ORDER eOrder, const T& x);

    template<sort::ORDER ORDER>
    isize pushSorted(IAllocator* p, const T& x);

    [[nodiscard]] T& last() noexcept;

    [[nodiscard]] const T& last() const noexcept;

    [[nodiscard]] T& first() noexcept;

    [[nodiscard]] const T& first() const noexcept;

    T& pop() noexcept;

    void setSize(IAllocator* p, isize size);

    void setCap(IAllocator* p, isize cap);

    void swapWithLast(isize i) noexcept;

    void popAsLast(isize i) noexcept;

    void removeAndShift(isize i) noexcept;

    [[nodiscard]] isize idx(const T* const x) const noexcept;

    [[nodiscard]] isize lastI() const noexcept;

    void destroy(IAllocator* p) noexcept;

    [[nodiscard]] Vec<T> release() noexcept;

    [[nodiscard]] isize size() const noexcept;

    [[nodiscard]] isize cap() const noexcept;

    [[nodiscard]] T* data() noexcept;

    [[nodiscard]] const T* data() const noexcept;

    void zeroOut() noexcept; /* set size to zero and memset */

    [[nodiscard]] Vec<T> clone(IAllocator* pAlloc) const;

    [[nodiscard]] bool search(const T& x) const;

    /* */

private:
    void grow(IAllocator* p, isize newCapacity);

    void growIfNeeded(IAllocator* p);
    void growOnSpanPush(IAllocator* p, const isize spanSize);

public:

    /* */

    T* begin() noexcept { return {&m_pData[0]}; }
    T* end() noexcept { return {&m_pData[m_size]}; }
    T* rbegin() noexcept { return {&m_pData[m_size - 1]}; }
    T* rend() noexcept { return {m_pData - 1}; }

    const T* begin() const noexcept { return {&m_pData[0]}; }
    const T* end() const noexcept { return {&m_pData[m_size]}; }
    const T* rbegin() const noexcept { return {&m_pData[m_size - 1]}; }
    const T* rend() const noexcept { return {m_pData - 1}; }
};

template<typename T>
inline
Vec<T>::Vec(IAllocator* p, isize prealloc, const T& defaultVal)
    : Vec(p, prealloc)
{
    setSize(p, prealloc);
    for (auto& e : (*this))
        e = defaultVal;
}

template<typename T>
inline isize
Vec<T>::fakePush(IAllocator* p)
{
    growIfNeeded(p);
    return ++m_size - 1;
}

template<typename T>
inline isize
Vec<T>::push(IAllocator* p, const T& data)
{
    growIfNeeded(p);
    new(m_pData + m_size++) T(data);
    return m_size - 1;
}

template<typename T>
inline void
Vec<T>::pushAt(IAllocator* p, const isize atI, const T& data)
{
    growIfNeeded(p);
    ADT_ASSERT(atI >= 0 && atI < size() + 1, "atI: {}, size + 1: {}", atI, size() + 1);

    utils::memMove<T>(m_pData + atI + 1, m_pData + atI, size() - atI);
    new(&operator[](atI)) T(data);

    ++m_size;
}

template<typename T>
inline isize
Vec<T>::pushSpan(IAllocator* p, const Span<const T> sp)
{
    growOnSpanPush(p, sp.size());
    utils::memCopy(m_pData + m_size, sp.data(), sp.size());

    const isize ret = m_size;
    m_size += sp.size();

    return ret;
}

template<typename T>
inline void
Vec<T>::pushSpanAt(IAllocator* p, const isize atI, const Span<const T> sp)
{
    growOnSpanPush(p, sp.size());
    m_size += sp.size();
    ADT_ASSERT(atI >= 0 && atI < size(), "atI: {}, size: {}", atI, size());

    utils::memMove(m_pData + atI + sp.size(), m_pData + atI, size() - sp.size() - atI);
    utils::memCopy(m_pData + atI, sp.data(), sp.size());
}

template<typename T>
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline isize
Vec<T>::emplace(IAllocator* p, ARGS&&... args)
{
    growIfNeeded(p);
    new(m_pData + m_size++) T(std::forward<ARGS>(args)...);
    return m_size - 1;
}

template<typename T>
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline void
Vec<T>::emplaceAt(IAllocator* p, const isize atI, ARGS&&... args)
{
    growIfNeeded(p);
    ++m_size;
    ADT_ASSERT(atI >= 0 && atI < size(), "atI: {}, size: {}", atI, size());

    utils::memMove(m_pData + atI + 1, m_pData + atI, size() - atI);
    new(&operator[](atI)) T(std::forward<ARGS>(args)...);
}

template<typename T>
inline isize
Vec<T>::pushSorted(IAllocator* p, const sort::ORDER eOrder, const T& x)
{
    return sort::push(p, eOrder, this, x);
}

template<typename T>
template<sort::ORDER ORDER>
inline isize
Vec<T>::pushSorted(IAllocator* p, const T& x)
{
    return sort::push<Vec<T>, T, ORDER>(p, this, x);
}

template<typename T>
inline T&
Vec<T>::last() noexcept
{
    return operator[](m_size - 1);
}

template<typename T>
inline const T&
Vec<T>::last() const noexcept
{
    return operator[](m_size - 1);
}

template<typename T>
inline T&
Vec<T>::first() noexcept
{
    return operator[](0);
}

template<typename T>
inline const T&
Vec<T>::first() const noexcept
{
    return operator[](0);
}

template<typename T>
inline T&
Vec<T>::pop() noexcept
{
    ADT_ASSERT(m_size > 0, "empty");
    return m_pData[--m_size];
}

template<typename T>
inline void
Vec<T>::setSize(IAllocator* p, isize size)
{
    if (m_capacity < size) grow(p, size);

    m_size = size;
}

template<typename T>
inline void
Vec<T>::setCap(IAllocator* p, isize cap)
{
    if (cap == 0)
    {
        destroy(p);
        return;
    }

    m_pData = (T*)p->realloc(m_pData, m_capacity, cap, sizeof(T));
    m_capacity = cap;

    if (m_size > cap) m_size = cap;
}

template<typename T>
inline void
Vec<T>::swapWithLast(isize i) noexcept
{
    ADT_ASSERT(m_size > 0, "empty");
    utils::swap(&operator[](i), &operator[](m_size - 1));
}

template<typename T>
inline void
Vec<T>::popAsLast(isize i) noexcept
{
    ADT_ASSERT(m_size > 0, "empty");
    operator[](i) = last();
    --m_size;
}

template<typename T>
inline void
Vec<T>::removeAndShift(isize i) noexcept
{
    ADT_ASSERT(m_size > 0, "empty");

    if (i != lastI())
        utils::memMove(&operator[](i), &operator[](i + 1), (m_size - i - 1));

    --m_size;
}

template<typename T>
inline isize
Vec<T>::idx(const T* const x) const noexcept
{
    isize r = isize(x - m_pData);
    ADT_ASSERT(r >= 0 && r < m_capacity,"r: {}, cap: {}, addr: {}. Must take the address of the reference", r, m_capacity, (void*)x);
    return r;
}

template<typename T>
inline isize
Vec<T>::lastI() const noexcept
{
    return idx(&last());
}

template<typename T>
inline void
Vec<T>::destroy(IAllocator* p) noexcept
{
    p->free(m_pData);
    *this = {};
}

template<typename T>
inline Vec<T>
Vec<T>::release() noexcept
{
    return utils::exchange(this, {});
}

template<typename T>
inline isize
Vec<T>::size() const noexcept
{
    return m_size;
}

template<typename T>
inline isize
Vec<T>::cap() const noexcept
{
    return m_capacity;
}

template<typename T>
inline T*
Vec<T>::data() noexcept
{
    return m_pData;
}

template<typename T>
inline const T*
Vec<T>::data() const noexcept
{
    return m_pData;
}

template<typename T>
inline void
Vec<T>::zeroOut() noexcept
{
    memset(m_pData, 0, m_size * sizeof(T));
}

template<typename T>
inline Vec<T>
Vec<T>::clone(IAllocator* pAlloc) const
{
    auto nVec = Vec<T>(pAlloc, cap());
    memcpy(nVec.data(), data(), size() * sizeof(T));
    nVec.m_size = size();

    return nVec;
}

template<typename T>
inline bool
Vec<T>::search(const T& x) const
{
    for (const auto& el : *this)
        if (el == x) return true;

    return false;
}

template<typename T>
inline void
Vec<T>::grow(IAllocator* p, isize newCapacity)
{
    m_pData = (T*)p->realloc(m_pData, m_capacity, newCapacity, sizeof(T));
    m_capacity = newCapacity;
}

template<typename T>
inline void
Vec<T>::growIfNeeded(IAllocator* p)
{
    if (m_size >= m_capacity)
    {
        isize newCap = utils::max(decltype(m_capacity)(SIZE_MIN), m_capacity * 2);
        ADT_ASSERT(newCap > m_capacity, "can't grow (capacity overflow), newCap: {}, m_capacity: {}", newCap, m_capacity);
        grow(p, newCap);
    }
}

template<typename T>
inline void
Vec<T>::growOnSpanPush(IAllocator* p, const isize spanSize)
{
    ADT_ASSERT(spanSize > 0, "pushing empty span");
    ADT_ASSERT(m_size + spanSize >= m_size, "overflow");

    if (m_size + spanSize > m_capacity)
    {
        const isize newSize = utils::max(static_cast<isize>((spanSize + m_size)*1.33), m_size*2);
        ADT_ASSERT(newSize > m_size, "overflow");
        grow(p, newSize);
    }
}

template<typename T, typename ALLOC_T = StdAllocatorNV>
struct VecManaged : protected ALLOC_T, Vec<T>
{
    using Base = Vec<T>;

    /* */

    VecManaged() = default;
    VecManaged(const isize prealloc) : ALLOC_T {}, Base {&allocator(), prealloc} {}
    VecManaged(const isize prealloc, const T& fillWith) : ALLOC_T {}, Base {&allocator(), prealloc, fillWith} {}

    /* */

    ALLOC_T& allocator() { return static_cast<ALLOC_T&>(*this); }
    const ALLOC_T& allocator() const { return static_cast<ALLOC_T&>(*this); }

    isize fakePush() { return Base::fakePush(&allocator()); }

    isize push(const T& data) { return Base::push(&allocator(), data); }

    void pushAt(const isize atI, const T& data) { Base::pushAt(&allocator(), atI,data); }

    isize pushSpan(const Span<const T> sp) { return Base::pushSpan(&allocator(), sp); }

    void pushSpanAt(const isize atI, const Span<const T> sp) { Base::pushSpanAt(&allocator(), atI, sp); }

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    isize emplace(ARGS&&... args) { return Base::emplace(&allocator(), std::forward<ARGS>(args)...); }

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    void emplaceAt(const isize atI, ARGS&&... args) { Base::emplaceAt(&allocator(), atI, std::forward<ARGS>(args)...); }

    isize pushSorted(const sort::ORDER eOrder, const T& x) { return Base::pushSorted(&allocator(), eOrder, x); }

    template<sort::ORDER ORDER>
    isize pushSorted(const T& x) { return Base::template pushSorted<ORDER>(&allocator(), x); }

    void setSize(isize size) { Base::setSize(&allocator(), size); }

    void setCap(isize cap) { Base::setCap(&allocator(), cap); }

    void destroy() noexcept { Base::destroy(&allocator()); }

    [[nodiscard]] VecManaged release() noexcept { return utils::exchange(this, {}); }

    [[nodiscard]] VecManaged
    clone()
    {
        VecManaged ret {&allocator(), Base::size()};
        ret.setSize(ret.cap());
        utils::memCopy(ret.data(), Base::data(), Base::size());

        return ret;
    }
};

} /* namespace adt */

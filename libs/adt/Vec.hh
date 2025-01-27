#pragma once

#include "IAllocator.hh"
#include "utils.hh"

#include <new> /* IWYU pragma: keep */
#include <utility>

namespace adt
{

#define ADT_VEC_FOREACH_I(A, I) for (ssize I = 0; I < (A)->size; ++I)
#define ADT_VEC_FOREACH_I_REV(A, I) for (ssize I = (A)->size - 1; I != -1U ; --I)

/* Dynamic array (aka Vector) */
template<typename T>
struct VecBase
{
    T* m_pData = nullptr;
    ssize m_size = 0;
    ssize m_capacity = 0;

    /* */

    VecBase() = default;
    VecBase(IAllocator* p, ssize prealloc = SIZE_MIN)
        : m_pData((T*)p->zalloc(prealloc, sizeof(T))),
          m_size(0),
          m_capacity(prealloc) {}

    /* */

#define ADT_RANGE_CHECK ADT_ASSERT(i >= 0 && i < m_size, "i: %lld, m_size: %lld", i, m_size);

    T& operator[](ssize i)             noexcept { ADT_RANGE_CHECK; return m_pData[i]; }
    const T& operator[](ssize i) const noexcept { ADT_RANGE_CHECK; return m_pData[i]; }

#undef ADT_RANGE_CHECK

    [[nodiscard]] bool empty() const noexcept { return m_size == 0; }

    ssize push(IAllocator* p, const T& data);

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
        ssize emplace(IAllocator* p, ARGS&&... args);

    [[nodiscard]] T& last() noexcept;

    [[nodiscard]] const T& last() const noexcept;

    [[nodiscard]] T& first() noexcept;

    [[nodiscard]] const T& first() const noexcept;

    T* pop() noexcept;

    void setSize(IAllocator* p, ssize size);

    void setCap(IAllocator* p, ssize cap);

    void swapWithLast(ssize i) noexcept;

    void popAsLast(ssize i) noexcept;

    void removeAndShift(ssize i) noexcept;

    [[nodiscard]] ssize idx(const T* x) const noexcept;

    [[nodiscard]] ssize lastI() const noexcept;

    void destroy(IAllocator* p) noexcept;

    [[nodiscard]] ssize getSize() const noexcept;

    [[nodiscard]] ssize getCap() const noexcept;

    [[nodiscard]] T* data() noexcept;

    [[nodiscard]] const T* data() const noexcept;

    void zeroOut() noexcept; /* set size to zero and memset */

    [[nodiscard]] VecBase<T> clone(IAllocator* pAlloc) const;

    /* */

private:
    void grow(IAllocator* p, ssize newCapacity);

    void growIfNeeded(IAllocator* p);

    /* */

public:
    struct It
    {
        T* s;

        It(T* pFirst) : s{pFirst} {}

        T& operator*() noexcept { return *s; }
        T* operator->() noexcept { return s; }

        It operator++() noexcept { ++s; return *this; }
        It operator++(int) noexcept { T* tmp = s++; return tmp; }

        It operator--() noexcept { --s; return *this; }
        It operator--(int) noexcept { T* tmp = s--; return tmp; }

        friend constexpr bool operator==(const It& l, const It& r) noexcept { return l.s == r.s; }
        friend constexpr bool operator!=(const It& l, const It& r) noexcept { return l.s != r.s; }
    };

    It begin() noexcept { return {&m_pData[0]}; }
    It end() noexcept { return {&m_pData[m_size]}; }
    It rbegin() noexcept { return {&m_pData[m_size - 1]}; }
    It rend() noexcept { return {m_pData - 1}; }

    const It begin() const noexcept { return {&m_pData[0]}; }
    const It end() const noexcept { return {&m_pData[m_size]}; }
    const It rbegin() const noexcept { return {&m_pData[m_size - 1]}; }
    const It rend() const noexcept { return {m_pData - 1}; }
};

template<typename T>
inline ssize
VecBase<T>::push(IAllocator* p, const T& data)
{
    growIfNeeded(p);
    new(m_pData + m_size++) T(data);
    return m_size - 1;
}

template<typename T>
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline ssize
VecBase<T>::emplace(IAllocator* p, ARGS&&... args)
{
    growIfNeeded(p);
    new(m_pData + m_size++) T(std::forward<ARGS>(args)...);
    return m_size - 1;
}

template<typename T>
inline T&
VecBase<T>::last() noexcept
{
    return operator[](m_size - 1);
}

template<typename T>
inline const T&
VecBase<T>::last() const noexcept
{
    return operator[](m_size - 1);
}

template<typename T>
inline T&
VecBase<T>::first() noexcept
{
    return operator[](0);
}

template<typename T>
inline const T&
VecBase<T>::first() const noexcept
{
    return operator[](0);
}

template<typename T>
inline T*
VecBase<T>::pop() noexcept
{
    ADT_ASSERT(m_size > 0, "empty");
    return &m_pData[--m_size];
}

template<typename T>
inline void
VecBase<T>::setSize(IAllocator* p, ssize size)
{
    if (m_capacity < size)
        grow(p, size);

    m_size = size;
}

template<typename T>
inline void
VecBase<T>::setCap(IAllocator* p, ssize cap)
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
VecBase<T>::swapWithLast(ssize i) noexcept
{
    ADT_ASSERT(m_size > 0, "empty");
    utils::swap(&operator[](i), &operator[](m_size - 1));
}

template<typename T>
inline void
VecBase<T>::popAsLast(ssize i) noexcept
{
    ADT_ASSERT(m_size > 0, "empty");
    operator[](i) = last();
    --m_size;
}

template<typename T>
inline void
VecBase<T>::removeAndShift(ssize i) noexcept
{
    ADT_ASSERT(m_size > 0, "empty");

    if (i != lastI())
        utils::move(&operator[](i), &operator[](i + 1), (m_size - i - 1));

    --m_size;
}

template<typename T>
inline ssize
VecBase<T>::idx(const T* x) const noexcept
{
    ssize r = ssize(x - m_pData);
    ADT_ASSERT(r >= 0 && r < m_capacity,"r: %lld, cap: %lld", r, m_capacity);
    return r;
}

template<typename T>
inline ssize
VecBase<T>::lastI() const noexcept
{
    return idx(&last());
}

template<typename T>
inline void
VecBase<T>::destroy(IAllocator* p) noexcept
{
    p->free(m_pData);
    *this = {};
}

template<typename T>
inline ssize
VecBase<T>::getSize() const noexcept
{
    return m_size;
}

template<typename T>
inline ssize
VecBase<T>::getCap() const noexcept
{
    return m_capacity;
}

template<typename T>
inline T*
VecBase<T>::data() noexcept
{
    return m_pData;
}

template<typename T>
inline const T*
VecBase<T>::data() const noexcept
{
    return m_pData;
}

template<typename T>
inline void
VecBase<T>::zeroOut() noexcept
{
    memset(m_pData, 0, m_size * sizeof(T));
}

template<typename T>
inline VecBase<T>
VecBase<T>::clone(IAllocator* pAlloc) const
{
    auto nVec = VecBase<T>(pAlloc, getCap());
    memcpy(nVec.data(), data(), getSize() * sizeof(T));
    nVec.m_size = getSize();

    return nVec;
}

template<typename T>
inline void
VecBase<T>::grow(IAllocator* p, ssize newCapacity)
{
    m_pData = (T*)p->realloc(m_pData, m_capacity, newCapacity, sizeof(T));
    m_capacity = newCapacity;
}

template<typename T>
inline void
VecBase<T>::growIfNeeded(IAllocator* p)
{
    if (m_size >= m_capacity)
    {
        ssize newCap = utils::max(decltype(m_capacity)(SIZE_MIN), m_capacity * 2);
        ADT_ASSERT(newCap > m_capacity, "can't grow (capacity overflow), newCap: %lld, m_capacity: %lld", newCap, m_capacity);
        grow(p, newCap);
    }
}

template<typename T>
struct Vec
{
    VecBase<T> base {};

    /* */

    IAllocator* m_pAlloc = nullptr;

    /* */

    Vec() = default;
    Vec(IAllocator* p, ssize prealloc = 1) : base(p, prealloc), m_pAlloc(p) {}

    /* */

    T& operator[](ssize i)             noexcept { return base[i]; }
    const T& operator[](ssize i) const noexcept { return base[i]; }

    [[nodiscard]] bool empty() const noexcept { return base.empty(); }

    ssize push(const T& data) { return base.push(m_pAlloc, data); }

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
        ssize emplace(ARGS&&... args) { return base.emplace(m_pAlloc, std::forward<ARGS>(args)...); }

    [[nodiscard]] T& last() noexcept { return base.last(); }

    [[nodiscard]] const T& last() const noexcept { return base.last(); }

    [[nodiscard]] T& first() noexcept { return base.first(); }

    [[nodiscard]] const T& first() const noexcept { return base.first(); }

    T* pop() noexcept { return base.pop(); }

    void setSize(ssize size) { base.setSize(m_pAlloc, size); }

    void setCap(ssize cap) { base.setCap(m_pAlloc, cap); }

    void swapWithLast(ssize i) noexcept { base.swapWithLast(i); }

    void popAsLast(ssize i) noexcept { base.popAsLast(i); }

    void removeAndShift(ssize i) noexcept { base.removeAndShift(i); }

    [[nodiscard]] ssize idx(const T* x) const noexcept { return base.idx(x); }

    [[nodiscard]] ssize lastI() const noexcept { return base.lastI(); }

    void destroy() { base.destroy(m_pAlloc); }

    [[nodiscard]] ssize getSize() const noexcept { return base.getSize(); }

    [[nodiscard]] ssize getCap() const noexcept { return base.getCap(); }

    [[nodiscard]] T* data() noexcept { return base.data(); }

    [[nodiscard]] const T* data() const noexcept { return base.data(); }

    void zeroOut() noexcept { base.zeroOut(); }

    [[nodiscard]] Vec<T>
    clone(IAllocator* pAlloc)
    {
        auto nBase = base.clone(pAlloc);
        Vec<T> nVec;
        nVec.base = nBase;
        nVec.m_pAlloc = pAlloc;
        return nVec;
    }

    /* */

    typename VecBase<T>::It begin()  noexcept { return base.begin(); }
    typename VecBase<T>::It end()    noexcept { return base.end(); }
    typename VecBase<T>::It rbegin() noexcept { return base.rbegin(); }
    typename VecBase<T>::It rend()   noexcept { return base.rend(); }

    const typename VecBase<T>::It begin()  const noexcept { return base.begin(); }
    const typename VecBase<T>::It end()    const noexcept { return base.end(); }
    const typename VecBase<T>::It rbegin() const noexcept { return base.rbegin(); }
    const typename VecBase<T>::It rend()   const noexcept { return base.rend(); }
};

} /* namespace adt */

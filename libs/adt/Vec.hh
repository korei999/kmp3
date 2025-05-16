#pragma once

#include "IAllocator.hh"
#include "utils.hh"

#include <new> /* IWYU pragma: keep */
#include <utility>

namespace adt
{

#define ADT_VEC_FOREACH_I(A, I) for (isize I = 0; I < (A)->size; ++I)
#define ADT_VEC_FOREACH_I_REV(A, I) for (isize I = (A)->size - 1; I != -1U ; --I)

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

    explicit operator Span<T>() const { return {data(), size()}; }

    /* */

#define ADT_RANGE_CHECK ADT_ASSERT(i >= 0 && i < m_size, "i: {}, m_size: {}", i, m_size);

    T& operator[](isize i)             noexcept { ADT_RANGE_CHECK; return m_pData[i]; }
    const T& operator[](isize i) const noexcept { ADT_RANGE_CHECK; return m_pData[i]; }

#undef ADT_RANGE_CHECK

    [[nodiscard]] bool empty() const noexcept { return m_size <= 0; }

    isize push(IAllocator* p, const T& data);

    void pushSpan(IAllocator* p, const Span<T> sp);

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
        isize emplace(IAllocator* p, ARGS&&... args);

    [[nodiscard]] T& last() noexcept;

    [[nodiscard]] const T& last() const noexcept;

    [[nodiscard]] T& first() noexcept;

    [[nodiscard]] const T& first() const noexcept;

    T* pop() noexcept;

    void setSize(IAllocator* p, isize size);

    void setCap(IAllocator* p, isize cap);

    void swapWithLast(isize i) noexcept;

    void popAsLast(isize i) noexcept;

    void removeAndShift(isize i) noexcept;

    [[nodiscard]] isize idx(const T* const x) const noexcept;

    [[nodiscard]] isize lastI() const noexcept;

    void destroy(IAllocator* p) noexcept;

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
Vec<T>::push(IAllocator* p, const T& data)
{
    growIfNeeded(p);
    new(m_pData + m_size++) T(data);
    return m_size - 1;
}

template<typename T>
inline void
Vec<T>::pushSpan(IAllocator* p, const Span<T> sp)
{
    ADT_ASSERT(sp.size() > 0, "pushing empty span");
    ADT_ASSERT(m_size + sp.size() >= m_size, "overflow");

    if (m_size + sp.size() > m_capacity)
    {
        isize newSize = utils::max(static_cast<isize>((sp.size() + m_size)*1.33), m_size*2);
        ADT_ASSERT(newSize > m_size, "overflow");
        grow(p, newSize);
    }

    utils::memCopy(m_pData + m_size, sp.data(), sp.size());
    m_size += sp.size();
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
inline T*
Vec<T>::pop() noexcept
{
    ADT_ASSERT(m_size > 0, "empty");
    return &m_pData[--m_size];
}

template<typename T>
inline void
Vec<T>::setSize(IAllocator* p, isize size)
{
    if (m_capacity < size)
        grow(p, size);

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
        if (el == x)
            return true;
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
struct VecManaged : Vec<T>
{
    IAllocator* m_pAlloc = nullptr;

    /* */

    using Vec<T>::Vec;

    VecManaged() = default;
    VecManaged(IAllocator* p, isize prealloc = 1) : Vec<T>(p, prealloc), m_pAlloc(p) {}
    VecManaged(IAllocator* p, isize preallocSize, const T& fillWith) : Vec<T>(p, preallocSize, fillWith) {}

    /* */

    isize push(const T& data) { return Vec<T>::push(m_pAlloc, data); }

    void pushSpan(const Span<T> sp) { Vec<T>::pushSpan(m_pAlloc, sp); }

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
        isize emplace(ARGS&&... args) { return Vec<T>::emplace(m_pAlloc, std::forward<ARGS>(args)...); }

    void setSize(isize size) { Vec<T>::setSize(m_pAlloc, size); }

    void setCap(isize cap) { Vec<T>::setCap(m_pAlloc, cap); }

    void destroy() { Vec<T>::destroy(m_pAlloc); }

    [[nodiscard]] VecManaged<T>
    clone(IAllocator* pAlloc)
    {
        Vec<T> nBase = Vec<T>::clone(pAlloc);
        VecManaged<T> nVec;
        new(&nVec) Vec<T> {nBase};
        nVec.m_pAlloc = pAlloc;
        return nVec;
    }
};

} /* namespace adt */

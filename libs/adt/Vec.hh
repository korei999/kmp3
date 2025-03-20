#pragma once

#include "IAllocator.hh"
#include "Span.hh"
#include "utils.hh"

#include <new> /* IWYU pragma: keep */
#include <utility>

namespace adt
{

#define ADT_VEC_FOREACH_I(A, I) for (ssize I = 0; I < (A)->size; ++I)
#define ADT_VEC_FOREACH_I_REV(A, I) for (ssize I = (A)->size - 1; I != -1U ; --I)

/* TODO: overflow checks are pretty naive */

/* Dynamic array (aka Vector) */
template<typename T>
struct Vec
{
    T* m_pData = nullptr;
    ssize m_size = 0;
    ssize m_capacity = 0;

    /* */

    Vec() = default;

    Vec(IAllocator* p, ssize prealloc = SIZE_MIN)
        : m_pData((T*)p->zalloc(prealloc, sizeof(T))),
          m_size(0),
          m_capacity(prealloc) {}

    Vec(IAllocator* p, ssize preallocSize, const T& fillWith);

    /* */

    explicit operator Span<T>() const { return {data(), size()}; }

    /* */

#define ADT_RANGE_CHECK ADT_ASSERT(i >= 0 && i < m_size, "i: %lld, m_size: %lld", i, m_size);

    T& operator[](ssize i)             noexcept { ADT_RANGE_CHECK; return m_pData[i]; }
    const T& operator[](ssize i) const noexcept { ADT_RANGE_CHECK; return m_pData[i]; }

#undef ADT_RANGE_CHECK

    [[nodiscard]] bool empty() const noexcept { return m_size <= 0; }

    ssize push(IAllocator* p, const T& data);

    void pushSpan(IAllocator* p, const Span<T> sp);

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

    [[nodiscard]] ssize idx(const T* const x) const noexcept;

    [[nodiscard]] ssize lastI() const noexcept;

    void destroy(IAllocator* p) noexcept;

    [[nodiscard]] ssize size() const noexcept;

    [[nodiscard]] ssize cap() const noexcept;

    [[nodiscard]] T* data() noexcept;

    [[nodiscard]] const T* data() const noexcept;

    void zeroOut() noexcept; /* set size to zero and memset */

    [[nodiscard]] Vec<T> clone(IAllocator* pAlloc) const;

    [[nodiscard]] bool search(const T& x) const;

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
inline
Vec<T>::Vec(IAllocator* p, ssize prealloc, const T& defaultVal)
    : Vec(p, prealloc)
{
    setSize(p, prealloc);
    for (auto& e : (*this))
        e = defaultVal;
}

template<typename T>
inline ssize
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
        ssize newSize = utils::max(static_cast<ssize>((sp.size() + m_size)*1.33), m_size*2);
        ADT_ASSERT(newSize > m_size, "overflow");
        grow(p, newSize);
    }

    utils::memCopy(m_pData + m_size, sp.data(), sp.size());
    m_size += sp.size();
}

template<typename T>
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline ssize
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
Vec<T>::setSize(IAllocator* p, ssize size)
{
    if (m_capacity < size)
        grow(p, size);

    m_size = size;
}

template<typename T>
inline void
Vec<T>::setCap(IAllocator* p, ssize cap)
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
Vec<T>::swapWithLast(ssize i) noexcept
{
    ADT_ASSERT(m_size > 0, "empty");
    utils::swap(&operator[](i), &operator[](m_size - 1));
}

template<typename T>
inline void
Vec<T>::popAsLast(ssize i) noexcept
{
    ADT_ASSERT(m_size > 0, "empty");
    operator[](i) = last();
    --m_size;
}

template<typename T>
inline void
Vec<T>::removeAndShift(ssize i) noexcept
{
    ADT_ASSERT(m_size > 0, "empty");

    if (i != lastI())
        utils::memMove(&operator[](i), &operator[](i + 1), (m_size - i - 1));

    --m_size;
}

template<typename T>
inline ssize
Vec<T>::idx(const T* const x) const noexcept
{
    ssize r = ssize(x - m_pData);
    ADT_ASSERT(r >= 0 && r < m_capacity,"r: %lld, cap: %lld, addr: %p. Must take the address of the reference", r, m_capacity, (void*)x);
    return r;
}

template<typename T>
inline ssize
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
inline ssize
Vec<T>::size() const noexcept
{
    return m_size;
}

template<typename T>
inline ssize
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
Vec<T>::grow(IAllocator* p, ssize newCapacity)
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
        ssize newCap = utils::max(decltype(m_capacity)(SIZE_MIN), m_capacity * 2);
        ADT_ASSERT(newCap > m_capacity, "can't grow (capacity overflow), newCap: %lld, m_capacity: %lld", newCap, m_capacity);
        grow(p, newCap);
    }
}

template<typename T>
struct VecManaged
{
    Vec<T> base {};

    /* */

    IAllocator* m_pAlloc = nullptr;

    /* */

    VecManaged() = default;
    VecManaged(IAllocator* p, ssize prealloc = 1) : base(p, prealloc), m_pAlloc(p) {}
    VecManaged(IAllocator* p, ssize preallocSize, const T& fillWith) : base(p, preallocSize, fillWith) {}

    /* */

    explicit operator Span<T>() const { return {Span<T>(base)}; }

    /* */

    T& operator[](ssize i)             noexcept { return base[i]; }
    const T& operator[](ssize i) const noexcept { return base[i]; }

    [[nodiscard]] bool empty() const noexcept { return base.empty(); }

    ssize push(const T& data) { return base.push(m_pAlloc, data); }

    void pushSpan(const Span<T> sp) { base.pushSpan(m_pAlloc, sp); }

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

    [[nodiscard]] ssize idx(const T* const x) const noexcept { return base.idx(x); }

    [[nodiscard]] ssize lastI() const noexcept { return base.lastI(); }

    void destroy() { base.destroy(m_pAlloc); }

    [[nodiscard]] ssize size() const noexcept { return base.size(); }

    [[nodiscard]] ssize cap() const noexcept { return base.cap(); }

    [[nodiscard]] T* data() noexcept { return base.data(); }

    [[nodiscard]] const T* data() const noexcept { return base.data(); }

    void zeroOut() noexcept { base.zeroOut(); }

    [[nodiscard]] VecManaged<T>
    clone(IAllocator* pAlloc)
    {
        auto nBase = base.clone(pAlloc);
        VecManaged<T> nVec;
        nVec.base = nBase;
        nVec.m_pAlloc = pAlloc;
        return nVec;
    }

    [[nodiscard]] bool search(const T& x) const { return base.search(x); }

    /* */

    typename Vec<T>::It begin()  noexcept { return base.begin(); }
    typename Vec<T>::It end()    noexcept { return base.end(); }
    typename Vec<T>::It rbegin() noexcept { return base.rbegin(); }
    typename Vec<T>::It rend()   noexcept { return base.rend(); }

    const typename Vec<T>::It begin()  const noexcept { return base.begin(); }
    const typename Vec<T>::It end()    const noexcept { return base.end(); }
    const typename Vec<T>::It rbegin() const noexcept { return base.rbegin(); }
    const typename Vec<T>::It rend()   const noexcept { return base.rend(); }
};

} /* namespace adt */

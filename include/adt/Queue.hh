#pragma once

#include "StdAllocator.hh"
#include "utils.hh"
#include "assert.hh"
#include "defer.hh"

namespace adt
{

template<typename T>
struct Queue
{
    T* m_pData;
    isize m_size;
    isize m_cap;
    isize m_headI;
    isize m_tailI;

    /* */

    Queue() : m_pData {}, m_size {}, m_cap {}, m_headI {}, m_tailI {} {}
    Queue(IAllocator* pAlloc, isize prealloc = SIZE_MIN);

    /* */

    T& operator[](isize i);
    const T& operator[](isize i) const;

    isize nextI(isize i) const;
    isize prevI(isize i) const;

    isize idx(const T* pElement) const;
    isize size() const;
    isize cap() const;
    bool empty() const;

    isize pushBack(IAllocator* pAlloc, const T& x);
    isize pushFront(IAllocator* pAlloc, const T& x);

    T popFront();
    T popBack();

    template<typename ...ARGS>
    isize emplaceFront(IAllocator* pAlloc, ARGS&&... args) { return pushFront(pAlloc, T(std::forward<ARGS>(args)...)); }

    template<typename ...ARGS>
    isize emplaceBack(IAllocator* pAlloc, ARGS&&... args) { return pushBack(pAlloc, T(std::forward<ARGS>(args)...)); }

    void destroy(IAllocator* pAlloc) noexcept;
    [[nodiscard]] Queue release() noexcept;

protected:
    void grow(IAllocator* pAlloc);

public:

    /* */

    struct It
    {
        Queue* m_p {};
        isize m_i {};
        isize m_count {};

        /* */

        It(const Queue* p, const isize i) : m_p {const_cast<Queue*>(p)}, m_i {i} {}
        It(const Queue* p, const isize i, const isize count) : m_p {const_cast<Queue*>(p)}, m_i {i}, m_count {count} {}

        /* */

        T& operator*() noexcept { return m_p->operator[](m_i); }
        T* operator->() noexcept { return &m_p->operator[](m_i); }

        It
        operator++() noexcept
        {
            m_i = m_p->nextI(m_i);
            ++m_count;
            return *this;
        };

        It
        operator--() noexcept
        {
            m_i = m_p->prevI(m_i);
            ++m_count;
            return *this;
        };

        friend bool operator==(const It l, const It r) { return l.m_count == r.m_count; }
        friend bool operator!=(const It l, const It r) { return l.m_count != r.m_count; }
    };

    It begin() noexcept { return {this, m_headI}; }
    It end() noexcept { return {this, m_tailI, m_size}; }
    const It begin() const noexcept { return {this, m_headI}; }
    const It end() const noexcept { return {this, m_tailI, m_size}; }

    It rbegin() noexcept { return {this, prevI(m_tailI)}; }
    It rend() noexcept { return {this, 0, m_size}; }
    const It rbegin() const noexcept { return {this, prevI(m_tailI)}; }
    const It rend() const noexcept { return {this, 0, m_size}; }
};

template<typename T>
inline
Queue<T>::Queue(IAllocator* pAlloc, isize prealloc)
    : m_size {}, m_headI {}, m_tailI {}
{
    const isize cap = nextPowerOf2(prealloc);
    ADT_ASSERT(isPowerOf2(cap), "nextPowerOf2: {}", cap);

    m_pData = pAlloc->zallocV<T>(cap);
    m_cap = cap;
}

template<typename T>
inline isize
Queue<T>::nextI(isize i) const
{
    return (i + 1) & (m_cap - 1);
}

template<typename T>
inline isize
Queue<T>::prevI(isize i) const
{
    return (i - 1) & (m_cap - 1);
}

template<typename T>
inline isize
Queue<T>::idx(const T* pElement) const
{
    ADT_ASSERT(pElement >= m_pData && pElement < m_pData + m_cap, "out of range pointer");
    return pElement - m_pData;
}

template<typename T>
inline isize
Queue<T>::size() const
{
    return m_size;
}

template<typename T>
inline isize
Queue<T>::cap() const
{
    return m_cap;
}

template<typename T>
inline bool
Queue<T>::empty() const
{
    return m_size <= 0;
}

template<typename T>
inline isize
Queue<T>::pushBack(IAllocator* pAlloc, const T& x)
{
    if (m_size >= m_cap)
        grow(pAlloc);

    isize nextTailI = nextI(m_tailI);

    const isize tmp = m_tailI;
    m_tailI = nextTailI;

    new(m_pData + tmp) T(x);
    ++m_size;
    return tmp;
}

template<typename T>
inline isize
Queue<T>::pushFront(IAllocator* pAlloc, const T& x)
{
    if (m_size >= m_cap)
        grow(pAlloc);

    m_headI = prevI(m_headI);
    new(m_pData + m_headI) T(x);

    ++m_size;
    return m_headI;
}

template<typename T>
inline T
Queue<T>::popFront()
{
    ADT_ASSERT(!empty(), "empty");

    const isize tmp = m_headI;
    m_headI = nextI(m_headI);

    --m_size;

    T ret = std::move(m_pData[tmp]);
    if constexpr (!std::is_trivially_destructible_v<T>)
        m_pData[tmp].~T();

    return ret;
}

template<typename T>
inline T
Queue<T>::popBack()
{
    ADT_ASSERT(!empty(), "empty");

    m_tailI = prevI(m_tailI);
    --m_size;

    T ret = std::move(m_pData[m_tailI]);
    if constexpr (!std::is_trivially_destructible_v<T>)
        m_pData[m_tailI].~T();

    return ret;
}

template<typename T>
inline void
Queue<T>::destroy(IAllocator* pAlloc) noexcept
{
    pAlloc->dealloc(m_pData, m_size);
    *this = {};
}

template<typename T>
inline Queue<T>
Queue<T>::release() noexcept
{
    return utils::exchange(this, {});
}

template<typename T>
inline void
Queue<T>::grow(IAllocator* pAlloc)
{
    const isize newCap = utils::max(isize(2), m_cap * 2);
    m_pData = pAlloc->relocate<T>(m_pData, m_cap, newCap);

    defer( m_cap = newCap );

    if (m_size <= 0) return;

    defer(
        m_headI = m_size;
        m_tailI = 0;
    );

    if (m_headI < m_tailI) /* sequential case */
    {
        utils::memCopy(m_pData + m_cap, m_pData + m_headI, m_cap);

        return;
    }
    else /* `m_headI == m_tailI` case */
    {
        const isize headToEndSize = m_size - m_headI;
        /* 1st: from head to the end of the buffer */
        utils::memCopy(m_pData + m_cap, m_pData + m_headI, headToEndSize);

        /* 2nd: from the start of the buffer to the tail */
        utils::memCopy(m_pData + m_cap + headToEndSize, m_pData, m_size - headToEndSize);

        return;
    }
}

template<typename T>
inline T&
Queue<T>::operator[](isize i)
{
    ADT_ASSERT(i >= 0 && i < m_cap, "out of capacity: i: {}, cap: {}", i, m_cap);
    return m_pData[i];
}

template<typename T>
inline const T&
Queue<T>::operator[](isize i) const
{
    ADT_ASSERT(i >= 0 && i < m_cap, "out of capacity: i: {}, cap: {}", i, m_cap);
    return m_pData[i];
}

template<typename T, typename ALLOC_T = StdAllocatorNV>
struct QueueManaged : protected ALLOC_T, public Queue<T>
{
    using Base = Queue<T>;

    /* */

    QueueManaged() = default;
    QueueManaged(isize prealloc) : Base {&allocator(), prealloc} {}

    /* */

    ALLOC_T& allocator() { return *static_cast<ALLOC_T*>(this); }
    const ALLOC_T& allocator() const { return *static_cast<ALLOC_T*>(this); }

    isize pushBack(const T& x) { return Base::pushBack(&allocator(), x); }
    isize pushFront(const T& x) { return Base::pushFront(&allocator(), x); }

    template<typename ...ARGS>
    isize emplaceFront(ARGS&&... args) { return Base::pushFront(&allocator(), T(std::forward<ARGS>(args)...)); }

    template<typename ...ARGS>
    isize emplaceBack(ARGS&&... args) { return Base::pushBack(&allocator(), T(std::forward<ARGS>(args)...)); }

    void destroy() noexcept { Base::destroy(&allocator()); }
    [[nodiscard]] QueueManaged release() noexcept { return utils::exchange(this, {}); }
};

} /* namespace adt */

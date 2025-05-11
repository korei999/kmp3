#pragma once

#include "IAllocator.hh"
#include "utils.hh"
#include "assert.hh"
#include "defer.hh"

#include <new>

namespace adt
{

template<typename T>
struct Queue
{
    T* m_pData;
    ssize m_size;
    ssize m_cap;
    ssize m_headI;
    ssize m_tailI;

    /* */

    Queue() : m_pData {}, m_size {}, m_cap {}, m_headI {}, m_tailI {} {}
    Queue(IAllocator* pAlloc, ssize prealloc = SIZE_MIN);

    /* */

    T& operator[](ssize i);
    const T& operator[](ssize i) const;

    ssize nextI(ssize i) const;
    ssize prevI(ssize i) const;

    ssize idx(const T* pElement) const;
    ssize size() const;
    ssize cap() const;
    bool empty() const;

    ssize pushBack(IAllocator* pAlloc, const T& x);
    ssize pushFront(IAllocator* pAlloc, const T& x);

    T* popFront();
    T* popBack();

    template<typename ...ARGS>
    ssize emplaceFront(IAllocator* pAlloc, ARGS&&... args) { return pushFront(pAlloc, T(std::forward<ARGS>(args)...)); }

    template<typename ...ARGS>
    ssize emplaceBack(IAllocator* pAlloc, ARGS&&... args) { return pushBack(pAlloc, T(std::forward<ARGS>(args)...)); }

protected:
    void grow(IAllocator* pAlloc);

public:

    /* */

    struct It
    {
        Queue* m_p {};
        ssize m_i {};
        ssize m_count {};

        /* */

        It(const Queue* p, const ssize i) : m_p {const_cast<Queue*>(p)}, m_i {i} {}
        It(const Queue* p, const ssize i, const ssize count) : m_p {const_cast<Queue*>(p)}, m_i {i}, m_count {count} {}

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
Queue<T>::Queue(IAllocator* pAlloc, ssize prealloc)
    : m_size {}, m_headI {}, m_tailI {}
{
    const ssize cap = nextPowerOf2(prealloc);
    ADT_ASSERT(isPowerOf2(cap), "nextPowerOf2: {}", cap);

    m_pData = pAlloc->mallocV<T>(cap);
    m_cap = cap;
}

template<typename T>
inline ssize
Queue<T>::nextI(ssize i) const
{
    return (i + 1) & (m_cap - 1);
}

template<typename T>
inline ssize
Queue<T>::prevI(ssize i) const
{
    return (i - 1) & (m_cap - 1);
}

template<typename T>
inline ssize
Queue<T>::idx(const T* pElement) const
{
    ADT_ASSERT(pElement >= m_pData && pElement < m_pData + m_cap, "out of range pointer");
    return pElement - m_pData;
}

template<typename T>
inline ssize
Queue<T>::size() const
{
    return m_size;
}

template<typename T>
inline ssize
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
inline ssize
Queue<T>::pushBack(IAllocator* pAlloc, const T& x)
{
    if (m_size >= m_cap)
        grow(pAlloc);

    ssize nextTailI = nextI(m_tailI);

    const ssize tmp = m_tailI;
    m_tailI = nextTailI;

    new(m_pData + tmp) T(x);
    ++m_size;
    return tmp;
}

template<typename T>
inline ssize
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
inline T*
Queue<T>::popFront()
{
    ADT_ASSERT(!empty(), "empty");

    const ssize tmp = m_headI;
    m_headI = nextI(m_headI);

    --m_size;
    return &m_pData[tmp];
}

template<typename T>
inline T*
Queue<T>::popBack()
{
    ADT_ASSERT(!empty(), "empty");

    m_tailI = prevI(m_tailI);
    --m_size;

    return &m_pData[m_tailI];
}

template<typename T>
inline void
Queue<T>::grow(IAllocator* pAlloc)
{
    const ssize newCap = utils::max(ssize(2), m_cap * 2);
    m_pData = pAlloc->reallocV<T>(m_pData, m_cap, newCap);

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
        const ssize headToEndSize = m_size - m_headI;
        /* 1st: from head to the end of the buffer */
        utils::memCopy(m_pData + m_cap, m_pData + m_headI, headToEndSize);

        /* 2nd: from the start of the buffer to the tail */
        utils::memCopy(m_pData + m_cap + headToEndSize, m_pData, m_size - headToEndSize);

        return;
    }
}

template<typename T>
inline T&
Queue<T>::operator[](ssize i)
{
    ADT_ASSERT(i >= 0 && i < m_cap, "out of capacity: i: {}, cap: {}", i, m_cap);
    return m_pData[i];
}

template<typename T>
inline const T&
Queue<T>::operator[](ssize i) const
{
    ADT_ASSERT(i >= 0 && i < m_cap, "out of capacity: i: {}, cap: {}", i, m_cap);
    return m_pData[i];
}

template<typename T>
struct QueueManaged : public Queue<T>
{
    IAllocator* m_pAlloc {};

    /* */

    QueueManaged() : Queue<T>::Queue() {}
    QueueManaged(IAllocator* pAlloc, ssize prealloc = SIZE_MIN)
        : Queue<T>::Queue(pAlloc, prealloc), m_pAlloc {pAlloc} {}

    /* */

    ssize pushBack(const T& x) { return Queue<T>::pushBack(m_pAlloc, x); }
    ssize pushFront(const T& x) { return Queue<T>::pushFront(m_pAlloc, x); }

    template<typename ...ARGS>
    ssize emplaceFront(ARGS&&... args) { return Queue<T>::pushFront(m_pAlloc, T(std::forward<ARGS>(args)...)); }

    template<typename ...ARGS>
    ssize emplaceBack(ARGS&&... args) { return Queue<T>::pushBack(m_pAlloc, T(std::forward<ARGS>(args)...)); }
};

} /* namespace adt */

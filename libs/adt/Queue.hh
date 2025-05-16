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

    T* popFront();
    T* popBack();

    template<typename ...ARGS>
    isize emplaceFront(IAllocator* pAlloc, ARGS&&... args) { return pushFront(pAlloc, T(std::forward<ARGS>(args)...)); }

    template<typename ...ARGS>
    isize emplaceBack(IAllocator* pAlloc, ARGS&&... args) { return pushBack(pAlloc, T(std::forward<ARGS>(args)...)); }

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

    m_pData = pAlloc->mallocV<T>(cap);
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
inline T*
Queue<T>::popFront()
{
    ADT_ASSERT(!empty(), "empty");

    const isize tmp = m_headI;
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
    const isize newCap = utils::max(isize(2), m_cap * 2);
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

template<typename T>
struct QueueManaged : public Queue<T>
{
    IAllocator* m_pAlloc {};

    /* */

    QueueManaged() : Queue<T>::Queue() {}
    QueueManaged(IAllocator* pAlloc, isize prealloc = SIZE_MIN)
        : Queue<T>::Queue(pAlloc, prealloc), m_pAlloc {pAlloc} {}

    /* */

    isize pushBack(const T& x) { return Queue<T>::pushBack(m_pAlloc, x); }
    isize pushFront(const T& x) { return Queue<T>::pushFront(m_pAlloc, x); }

    template<typename ...ARGS>
    isize emplaceFront(ARGS&&... args) { return Queue<T>::pushFront(m_pAlloc, T(std::forward<ARGS>(args)...)); }

    template<typename ...ARGS>
    isize emplaceBack(ARGS&&... args) { return Queue<T>::pushBack(m_pAlloc, T(std::forward<ARGS>(args)...)); }
};

} /* namespace adt */

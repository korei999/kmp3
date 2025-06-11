#pragma once

#include "Array.hh"

namespace adt
{

/* statically allocated reusable resource collection */
template<typename T, isize CAP>
struct Pool
{
    struct Handle
    {
        using ResourceType = T;

        /* */

        isize i = NPOS; /* index of m_aSlots */

        /* */

        explicit operator bool() const { return i != -1; }
    };


    T m_aSlots[CAP] {};
    Array<Handle, CAP> m_aFreeSlots {};
    bool m_aOccupied[CAP] {};
    isize m_nOccupied {};

    /* */

    ADT_WARN_INIT Pool() = default;
    Pool(InitFlag);

    /* */

    T& operator[](Handle h)             { return at(h); }
    const T& operator[](Handle h) const { return at(h); }

    isize firstI() const;
    isize lastI() const;
    isize nextI(isize i) const;
    isize prevI(isize i) const;

    isize idx(const T* const p) const;

    [[nodiscard]] Handle insert();
    [[nodiscard]] Handle insert(const T& value); /* push and construct */

    template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
    [[nodiscard]] Handle emplace(ARGS&&... args);

    void remove(Handle hnd);
    void remove(T* hnd);

    isize cap() const { return CAP; }
    isize size() const { return m_nOccupied; }

    bool empty() const { return size() == 0; }

    /* */

private:
    T& at(Handle h);

    /* */

public:
    struct It
    {
        Pool* s {};
        isize i {};

        /* */

        It(const Pool* _self, isize _i) : s(const_cast<Pool*>(_self)), i(_i) {}

        /* */

        auto& operator*() { return s->m_aSlots[i]; }
        auto* operator->() { return &s->m_aSlots[i]; }

        It
        operator++()
        {
            i = s->nextI(i);
            return {s, i};
        }

        It
        operator++(int)
        {
            isize tmp = i;
            i = s->nextI(i);
            return {s, tmp};
        }

        It
        operator--()
        {
            i = s->prevI(i);
            return {s, i};
        }

        It
        operator--(int)
        {
            isize tmp = i;
            i = s->prevI(i);
            return {s, tmp};
        }

        friend bool operator==(const It l, const It r) { return l.i == r.i; }
        friend bool operator!=(const It l, const It r) { return l.i != r.i; }
    };

    It begin() { return {this, firstI()}; }
    It end() { return {this, -1}; }

    const It begin() const { return {this, firstI()}; }
    const It end() const { return {this, -1}; }
};

template<typename T, isize CAP>
inline
Pool<T, CAP>::Pool(InitFlag)
{
    m_aFreeSlots.setSize(CAP);
    for (isize i = 0; i < CAP; ++i)
        m_aFreeSlots[i] = Handle {CAP - 1 - i};
}

template<typename T, isize CAP>
inline isize
Pool<T, CAP>::firstI() const
{
    if (m_aFreeSlots.size() >= CAP) return -1;

    for (isize i = 0; i < CAP; ++i)
        if (m_aOccupied[i]) return i;

    return -1;
}

template<typename T, isize CAP>
inline isize
Pool<T, CAP>::lastI() const
{
    if (m_aFreeSlots.size() >= CAP) return -1;

    for (isize i = CAP - 1; i >= 0; --i)
        if (m_aOccupied[i]) return i;

    return -1;
}

template<typename T, isize CAP>
inline isize
Pool<T, CAP>::nextI(isize i) const
{
    do if (++i >= CAP) return -1;
    while (!m_aOccupied[i]);

    return i;
}

template<typename T, isize CAP>
inline isize
Pool<T, CAP>::prevI(isize i) const
{
    do if (++i < 0) return -1;
    while (!m_aOccupied[i]);

    return i;
}

template<typename T, isize CAP>
inline isize
Pool<T, CAP>::idx(const T* const p) const
{
    isize r = p - &m_aSlots[0];
    ADT_ASSERT(r >= 0 && r < CAP, "out of range");
    return r;
}

template<typename T, isize CAP>
inline typename Pool<T, CAP>::Handle
Pool<T, CAP>::insert()
{
    Handle ret {.i = -1};

    if (m_aFreeSlots.empty())
    {
        print::err("pool is empty, returning -1\n");
        return ret;
    }

    ret = m_aFreeSlots.pop();
    m_aOccupied[ret.i] = true;
    ++m_nOccupied;

    return ret;
}

template<typename T, isize CAP>
inline typename Pool<T, CAP>::Handle
Pool<T, CAP>::insert(const T& value)
{
    auto idx = insert();
    new(&operator[](idx)) T(value);
    return idx;
}

template<typename T, isize CAP>
template<typename ...ARGS> requires(std::is_constructible_v<T, ARGS...>)
inline typename Pool<T, CAP>::Handle
Pool<T, CAP>::emplace(ARGS&&... args)
{
    auto idx = insert();
    new(&operator[](idx)) T(std::forward<ARGS>(args)...);
    return idx;
}

template<typename T, isize CAP>
inline void
Pool<T, CAP>::remove(Handle hnd)
{
    ADT_ASSERT(m_nOccupied > 0, "nothing to return");
    --m_nOccupied;

    m_aFreeSlots.push(hnd);
    ADT_ASSERT(m_aOccupied[hnd.i], "returning unoccupied node");
    m_aOccupied[hnd.i] = false;
}

template<typename T, isize CAP>
inline void
Pool<T, CAP>::remove(T* hnd)
{
    remove(Handle(idx(hnd)));
}

template<typename T, isize CAP>
inline T&
Pool<T, CAP>::at(Handle h)
{
    ADT_ASSERT(h.i >= 0 && h.i < CAP, "i: {}, CAP: {}", h.i, CAP);
    ADT_ASSERT(m_aOccupied[h.i], "trying to access unoccupied node");
    return m_aSlots[h.i];
}

namespace print
{

template<typename T, isize CAP>
inline isize
formatToContext(Context ctx, FormatArgs fmtArgs, const typename Pool<T, CAP>::Handle& x)
{
    return formatToContext(ctx, fmtArgs, x.i);
}

} /* namespace print */

} /* namespace adt */

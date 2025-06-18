#pragma once

#include "Array.hh"

namespace adt
{

template<typename STRUCT>
struct PoolSOAHandle
{
    using StructType = STRUCT;

    /* */

    int i = -1;

    /* */

    operator bool() const { return i != -1; }
};

template<typename STRUCT, int CAP, auto MEMBER>
struct SOAArrayHolder
{
    using MemberType = std::remove_reference_t<decltype(std::declval<STRUCT>().*MEMBER)>;

    /* */

    MemberType m_arrays[CAP] {};
};

template<typename STRUCT, typename BIND, int CAP, auto ...MEMBERS>
struct PoolSOA : public SOAArrayHolder<STRUCT, CAP, MEMBERS>...
{
    Array<PoolSOAHandle<STRUCT>, CAP> m_aFreeHandles {};
    bool m_aOccupiedIdxs[CAP] {};
    int m_size {};

    /* */

    void
    set(PoolSOAHandle<STRUCT> h, const STRUCT& x)
    {
        (new(&static_cast<SOAArrayHolder<STRUCT, CAP, MEMBERS>&>(*this).m_arrays[h.i])
          SOAArrayHolder<STRUCT, CAP, MEMBERS>::MemberType(x.*MEMBERS), ...);
    }

    BIND operator[](PoolSOAHandle<STRUCT> idx) { return BIND {bindMember<MEMBERS>(idx)...}; }
    const BIND operator[](PoolSOAHandle<STRUCT> idx) const { return BIND {bindMember<MEMBERS>(idx)...}; }

    [[nodiscard]] PoolSOAHandle<STRUCT>
    make(const STRUCT& x)
    {
        if (!m_aFreeHandles.empty())
        {
            PoolSOAHandle h = m_aFreeHandles.pop();
            m_aOccupiedIdxs[h.i] = true;
            set(h, x);
            return h;
        }
        else
        {
            if (m_size == CAP)
            {
#if !defined NDEBUG
                print::err("PoolSOA::make(): out of size, returning -1\n");
#endif
                return {.i = -1};
            }

            ++m_size;
            m_aOccupiedIdxs[m_size - 1] = true;
            set({m_size - 1}, x);
            return {m_size - 1};
        }
    }

    void
    giveBack(PoolSOAHandle<STRUCT> h)
    {
        m_aOccupiedIdxs[h.i] = false;
        m_aFreeHandles.push(h);
    }

    template<auto MEMBER>
    decltype(auto)
    bindMember(PoolSOAHandle<STRUCT> h)
    {
        ADT_ASSERT(h.i >= 0 && h.i < CAP, "out of range: h: {}, CAP: {}", h.i, CAP);
        ADT_ASSERT(m_aOccupiedIdxs[h.i], "handle '{}' is free", h.i);
        return static_cast<SOAArrayHolder<STRUCT, CAP, MEMBER>&>(*this).m_arrays[h.i];
    }

    template<auto MEMBER>
    decltype(auto)
    bindMember(PoolSOAHandle<STRUCT> h) const
    {
        ADT_ASSERT(h.i >= 0 && h.i < CAP, "out of range: h: {}, CAP: {}", h.i, CAP);
        ADT_ASSERT(m_aOccupiedIdxs[h.i], "handle '{}' is free", h.i);
        return static_cast<const SOAArrayHolder<STRUCT, CAP, MEMBER>&>(*this).m_arrays[h.i];
    }

    isize size() const { return static_cast<isize>(m_size); }
    isize cap() const { return static_cast<isize>(CAP); }

    int
    firstI() const
    {
        if (m_size == 0) return -1;

        for (isize i = 0; i < m_size; ++i)
            if (m_aOccupiedIdxs[i]) return i;

        return NPOS;
    }

    int
    lastI() const
    {
        if (m_size == 0) return -1;

        for (isize i = m_size - 1; i >= 0; --i)
            if (m_aOccupiedIdxs[i]) return i;

        return NPOS;
    }

    int
    nextI(int i) const
    {
        do ++i;
        while (i < m_size && !m_aOccupiedIdxs[i]);

        return i;
    }

    int
    prevI(int i) const
    {
        do --i;
        while (i >= 0 && !m_aOccupiedIdxs[i]);

        return i;
    }

    /* */

    struct It
    {
        PoolSOA* s {};
        int i {};

        /* */

        It(const PoolSOA* _s, int _i) : s(const_cast<PoolSOA*>(_s)), i(_i) {}

        /* */

        auto operator*() { return s->operator[]({i}); }

        friend bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend bool operator!=(const It& l, const It& r) { return l.i != r.i; }

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
    };

    It begin() { return {this, firstI()}; }
    It end() { return {this, size() == 0 ? -1 : lastI() + 1}; }

    const It begin() const { return {this, firstI()}; }
    const It end() const { return {this, size() == 0 ? -1 : lastI() + 1}; }
};

} /* namespace adt */

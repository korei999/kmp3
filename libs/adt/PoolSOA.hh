#pragma once

#include "Array.hh"

#include <type_traits>

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
    bool m_aNonFree[CAP] {};
    int m_size {};

    /* */

    void
    set(PoolSOAHandle<STRUCT> h, const STRUCT& x)
    {
        /* for each Member, assign .*Member to the corresponding struct element. */
        ((static_cast<SOAArrayHolder<STRUCT, CAP, MEMBERS>&>(*this).m_arrays[h.i] = x.*MEMBERS), ...);
    }

    BIND operator[](PoolSOAHandle<STRUCT> idx) { return BIND {bindMember<MEMBERS>(idx)...}; }
    const BIND operator[](PoolSOAHandle<STRUCT> idx) const { return BIND {bindMember<MEMBERS>(idx)...}; }

    [[nodiscard]] PoolSOAHandle<STRUCT>
    make(const STRUCT& x)
    {
        if (!m_aFreeHandles.empty())
        {
            PoolSOAHandle h = *m_aFreeHandles.pop();
            set(h, x);
            m_aNonFree[h.i] = false;
            return h;
        }
        else
        {
            if (m_size == CAP)
            {
#if !defined NDEBUG
                fprintf(stderr, "PoolSOA::make(): out of size, returning -1\n");
#endif
                return {.i = -1};
            }

            ++m_size;
            set({m_size - 1}, x);
            m_aNonFree[m_size - 1] = true;
            return {m_size - 1};
        }
    }

    void
    giveBack(PoolSOAHandle<STRUCT> h)
    {
        m_aNonFree[h.i] = false;
        m_aFreeHandles.push(h);
    }

private:
    template<auto MEMBER>
    decltype(auto)
    bindMember(PoolSOAHandle<STRUCT> h)
    {
        ADT_ASSERT(h.i >= 0 && h.i < CAP, "out of range: h: %d, CAP: %d", h.i, CAP);
        ADT_ASSERT(m_aNonFree[h.i], "handle '%d' is free", h.i);
        return static_cast<SOAArrayHolder<STRUCT, CAP, MEMBER>&>(*this).m_arrays[h.i];
    }

    template<auto MEMBER>
    decltype(auto)
    bindMember(PoolSOAHandle<STRUCT> h) const
    {
        ADT_ASSERT(h.i >= 0 && h.i < CAP, "out of range: h: %d, CAP: %d", h.i, CAP);
        ADT_ASSERT(m_aNonFree[h.i], "handle '%d' is free", h.i);
        return static_cast<const SOAArrayHolder<STRUCT, CAP, MEMBER>&>(*this).m_arrays[h.i];
    }
};

} /* namespace adt */

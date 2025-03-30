#pragma once

#include "types.hh"
#include "print.hh"

#if __has_include(<windows.h>)
    #define ADT_USE_WIN32_ATOMICS
#elif __has_include(<pthread.h>)
    #include <pthread.h>
    #define ADT_USE_LINUX_ATOMICS
#endif

namespace adt::atomic
{

enum class ORDER : int
{
    /* https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html */
    RELAXED, /* Implies no inter-thread ordering constraints. */
    CONSUME, /* This is currently implemented using the stronger __ATOMIC_ACQUIRE memory order because of a deficiency in C++11’s semantics for memory_order_consume. */
    ACQUIRE, /* Creates an inter-thread happens-before constraint from the release (or stronger) semantic store to this acquire load. Can prevent hoisting of code to before the operation. */
    RELEASE, /* Creates an inter-thread happens-before constraint to acquire (or stronger) semantic loads that read from this release store. Can prevent sinking of code to after the operation. */
    ACQ_REL, /* Combines the effects of both __ATOMIC_ACQUIRE and __ATOMIC_RELEASE. */
    SEQ_CST, /* Enforces total ordering with all other __ATOMIC_SEQ_CST operations. */
};

struct Int
{
#ifdef ADT_USE_WIN32_ATOMICS
    using Type = LONG;
#else
    using Type = i32;
#endif

    /* */

    volatile Type m_int {};

    /* */

    ADT_ALWAYS_INLINE Type
    load(const ORDER eOrder) const
    {
#ifdef ADT_USE_LINUX_ATOMICS

        return __atomic_load_n(&m_int, orderMap(eOrder));

#elif defined ADT_USE_WIN32_ATOMICS

        return InterlockedCompareExchange(const_cast<volatile LONG*>(&m_int), 0, 0);

#endif
    }

    ADT_ALWAYS_INLINE void
    store(const int val, const ORDER eOrder)
    {
#ifdef ADT_USE_LINUX_ATOMICS

        __atomic_store_n(&m_int, val, orderMap(eOrder));

#elif defined ADT_USE_WIN32_ATOMICS

        InterlockedExchange(&m_int, val);

#endif
    }

    ADT_ALWAYS_INLINE Type
    fetchAdd(const int val, const ORDER eOrder)
    {
#ifdef ADT_USE_LINUX_ATOMICS

        return __atomic_fetch_add(&m_int, val, orderMap(eOrder));

#elif defined ADT_USE_WIN32_ATOMICS

        return InterlockedExchangeAdd(&m_int, val);

#endif
    }

    ADT_ALWAYS_INLINE Type
    fetchSub(const int val, const ORDER eOrder)
    {
#ifdef ADT_USE_LINUX_ATOMICS

        return __atomic_fetch_sub(&m_int, val, orderMap(eOrder));

#elif defined ADT_USE_WIN32_ATOMICS

        return InterlockedExchangeAdd(&m_int, -val);

#endif
    }

    /* */
protected:

    ADT_ALWAYS_INLINE static constexpr int
    orderMap(const ORDER eOrder) noexcept
    {
#ifdef ADT_USE_LINUX_ATOMICS

        return static_cast<int>(eOrder);

#elif defined ADT_USE_WIN32_ATOMICS

        return {};

#endif
    }
};

} /* namespace adt::atomic */

namespace adt::print
{

inline ssize
formatToContext(Context ctx, FormatArgs fmtArgs, const atomic::Int& x) noexcept
{
    return formatToContext(ctx, fmtArgs, x.load(atomic::ORDER::RELAXED));
}

} /* namespace adt::print */

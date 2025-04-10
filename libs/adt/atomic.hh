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

/* https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html */
/* https://en.cppreference.com/w/c/atomic/memory_order */
enum class ORDER : int
{
    /* Relaxed operation: there are no synchronization or ordering constraints imposed on other reads or writes,
     * only this operation's atomicity is guaranteedsee Relaxed ordering below. */
    RELAXED,

    /* A load operation with this memory order performs a consume operation on the affected memory location:
     * no reads or writes in the current thread dependent on the value currently loaded can be reordered before this load.
     * Writes to data-dependent variables in other threads that release the same atomic variable are visible in the current thread.
     * On most platforms, this affects compiler optimizations only. */
    CONSUME,

    /* A load operation with this memory order performs the acquire operation on the affected memory location:
     * no reads or writes in the current thread can be reordered before this load.
     * All writes in other threads that release the same atomic variable are visible in the current thread. */
    ACQUIRE,

    /* A store operation with this memory order performs the release operation:
     * no reads or writes in the current thread can be reordered after this store.
     * All writes in the current thread are visible in other threads that acquire the same atomic variable and writes
     * that carry a dependency into the atomic variable become visible in other threads that consume the same atomic. */
    RELEASE,

    /* A read-modify-write operation with this memory order is both an acquire operation and a release operation.
     * No memory reads or writes in the current thread can be reordered before the load, nor after the store.
     * All writes in other threads that release the same atomic variable are visible before the modification
     * and the modification is visible in other threads that acquire the same atomic variable. */
    ACQ_REL,

    /* A load operation with this memory order performs an acquire operation,
     * a store performs a release operation, and read-modify-write performs both an acquire operation and a release operation,
     * plus a single total order exists in which all threads observe all modifications in the same order. */
    SEQ_CST,
};

struct Int
{
#ifdef ADT_USE_WIN32_ATOMICS
    using Type = LONG;
#else
    using Type = i32;
#endif

    /* */

    volatile Type m_int;

    /* */

    Int() { store(0, ORDER::RELAXED); }
    explicit Int(const int val) { store(val, ORDER::RELAXED); }

    /* */

    ADT_ALWAYS_INLINE Type
    load(const ORDER eOrder) const noexcept
    {
#ifdef ADT_USE_LINUX_ATOMICS

        return __atomic_load_n(&m_int, orderMap(eOrder));

#elif defined ADT_USE_WIN32_ATOMICS

        return InterlockedCompareExchange(const_cast<volatile LONG*>(&m_int), 0, 0);

#endif
    }

    ADT_ALWAYS_INLINE void
    store(const int val, const ORDER eOrder) noexcept
    {
#ifdef ADT_USE_LINUX_ATOMICS

        __atomic_store_n(&m_int, val, orderMap(eOrder));

#elif defined ADT_USE_WIN32_ATOMICS

        InterlockedExchange(&m_int, val);

#endif
    }

    ADT_ALWAYS_INLINE Type
    fetchAdd(const int val, const ORDER eOrder) noexcept
    {
#ifdef ADT_USE_LINUX_ATOMICS

        return __atomic_fetch_add(&m_int, val, orderMap(eOrder));

#elif defined ADT_USE_WIN32_ATOMICS

        return InterlockedExchangeAdd(&m_int, val);

#endif
    }

    ADT_ALWAYS_INLINE Type
    fetchSub(const int val, const ORDER eOrder) noexcept
    {
#ifdef ADT_USE_LINUX_ATOMICS

        return __atomic_fetch_sub(&m_int, val, orderMap(eOrder));

#elif defined ADT_USE_WIN32_ATOMICS

        return InterlockedExchangeAdd(&m_int, -val);

#endif
    }

    ADT_ALWAYS_INLINE Type
    compareExchangeWeak(Type* pExpected, Type desired, ORDER eSucces, ORDER eFailure) noexcept
    {
#ifdef ADT_USE_LINUX_ATOMICS

        return __atomic_compare_exchange_n(&m_int, pExpected, desired, true /* weak */, orderMap(eSucces), orderMap(eFailure));

#elif defined ADT_USE_WIN32_ATOMICS

        ADT_ASSERT(false, "not implemented");
        return {};

#endif
    };

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

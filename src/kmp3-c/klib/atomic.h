#pragma once

#include "common.h"

#if defined _WIN32

    #define K_THREAD_WIN32
    #define K_THREAD_LOCAL __declspec( thread )

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #include <windows.h>

    #undef MIN
    #undef MAX
    #undef NEAR
    #undef FAR

typedef LONG k_atomic_IntType;

#elif defined __unix__

typedef int k_atomic_IntType;

/* __ATOMIC_RELAXED
 * __ATOMIC_CONSUME
 * __ATOMIC_ACQUIRE
 * __ATOMIC_RELEASE
 * __ATOMIC_ACQ_REL
 * __ATOMIC_SEQ_CST */

/* https://en.cppreference.com/w/cpp/atomic/memory_order.html
 * memory_order_relaxed
 * Relaxed operation: there are no synchronization or ordering constraints imposed on other reads or writes,
 * only this operation's atomicity is guaranteed
 *
 * memory_order_consume (deprecated in C++26)
 * A load operation with this memory order performs a consume operation on the affected memory location:
 * no reads or writes in the current thread dependent on the value currently loaded can be reordered before this load.
 * Writes to data-dependent variables in other threads that release the same atomic variable are visible in the current thread.
 * On most platforms, this affects compiler optimizations only
 *
 * memory_order_acquire
 * A load operation with this memory order performs the acquire operation on the affected memory location:
 * no reads or writes in the current thread can be reordered before this load.
 * All writes in other threads that release the same atomic variable are visible in the current thread
 *
 * memory_order_release
 * A store operation with this memory order performs the release operation:
 * no reads or writes in the current thread can be reordered after this store.
 * All writes in the current thread are visible in other threads that acquire the same atomic variable
 * and writes that carry a dependency into the atomic variable become visible in other threads that consume the same atomic
 *
 * memory_order_acq_rel
 * A read-modify-write operation with this memory order is both an acquire operation and a release operation.
 * No memory reads or writes in the current thread can be reordered before the load, nor after the store.
 * All writes in other threads that release the same atomic variable
 * are visible before the modification and the modification is visible in other threads that acquire the same atomic variable.
 *
 * memory_order_seq_cst
 * A load operation with this memory order performs an acquire operation,
 * a store performs a release operation, and read-modify-write performs both an acquire operation and a release operation,
 * plus a single total order exists in which all threads observe all modifications in the same order */

#endif

typedef struct k_atomic_Int
{
    volatile k_atomic_IntType volNum;
} k_atomic_Int;

K_ALWAYS_INLINE static k_atomic_IntType k_atomic_IntLoadRelaxed(k_atomic_Int* s);
K_ALWAYS_INLINE static k_atomic_IntType k_atomic_IntLoadAcquire(k_atomic_Int* s);
K_ALWAYS_INLINE static void k_atomic_IntStoreRelease(k_atomic_Int* s, k_atomic_IntType val);
K_ALWAYS_INLINE static k_atomic_IntType k_atomic_IntAddRelaxed(k_atomic_Int* s, k_atomic_IntType val);
K_ALWAYS_INLINE static k_atomic_IntType k_atomic_IntAddRelease(k_atomic_Int* s, k_atomic_IntType val);
K_ALWAYS_INLINE static k_atomic_IntType k_atomic_IntSubRelaxed(k_atomic_Int* s, k_atomic_IntType val);
K_ALWAYS_INLINE static k_atomic_IntType k_atomic_IntSubRelease(k_atomic_Int* s, k_atomic_IntType val);
K_ALWAYS_INLINE static bool k_atomic_IntCASRelaxed(k_atomic_Int* s, k_atomic_IntType* pExpected, k_atomic_IntType desired);

typedef struct k_atomic_U64
{
    volatile uint64_t volNum;
} k_atomic_U64;

K_ALWAYS_INLINE static uint64_t k_atomic_U64LoadRelaxed(const k_atomic_U64* s);
K_ALWAYS_INLINE static uint64_t k_atomic_U64LoadAcquire(k_atomic_U64* s);
K_ALWAYS_INLINE static void k_atomic_U64StoreRelaxed(k_atomic_U64* s, uint64_t val);
K_ALWAYS_INLINE static void k_atomic_U64StoreRelease(k_atomic_U64* s, uint64_t val);
K_ALWAYS_INLINE static bool k_atomic_U64CASRelaxed(k_atomic_U64* s, uint64_t* pExpected, uint64_t desired);
K_ALWAYS_INLINE static bool k_atomic_U64CASRelease(k_atomic_U64* s, uint64_t* pExpected, uint64_t desired);
K_ALWAYS_INLINE static uint64_t k_atomic_U64AddRelaxed(k_atomic_U64* s, uint64_t val);
K_ALWAYS_INLINE static uint64_t k_atomic_U64AddRelease(k_atomic_U64* s, uint64_t val);
K_ALWAYS_INLINE static uint64_t k_atomic_U64SubRelaxed(k_atomic_U64* s, uint64_t val);
K_ALWAYS_INLINE static uint64_t k_atomic_U64SubRelease(k_atomic_U64* s, uint64_t val);

typedef struct k_atomic_U8
{
    volatile uint8_t volNum;
} k_atomic_U8;

K_ALWAYS_INLINE static uint8_t k_atomic_U8LoadRelaxed(const k_atomic_U8* s);
K_ALWAYS_INLINE static void k_atomic_U8StoreRelaxed(k_atomic_U8* s, uint8_t val);
K_ALWAYS_INLINE static void k_atomic_U8StoreRelease(k_atomic_U8* s, uint8_t val);
K_ALWAYS_INLINE static bool k_atomic_U8CASAcquire(k_atomic_U8* s, uint8_t* pExpected, uint8_t desired);
K_ALWAYS_INLINE static bool k_atomic_U8CASRelaxed(k_atomic_U8* s, uint8_t* pExpected, uint8_t desired);

#if defined _WIN32

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntLoadRelaxed(k_atomic_Int* s)
{
    return InterlockedCompareExchangeNoFence(&s->volNum, 0, 0);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntLoadAcquire(k_atomic_Int* s)
{
    return InterlockedCompareExchangeAcquire(&s->volNum, 0, 0);
}

K_ALWAYS_INLINE static void
k_atomic_IntStoreRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    InterlockedExchange(&s->volNum, val);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntAddRelaxed(k_atomic_Int* s, k_atomic_IntType val)
{
    return InterlockedAddNoFence(&s->volNum, val) - val;
}

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntAddRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    return InterlockedAddRelease(&s->volNum, val) - val;
}

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntSubRelaxed(k_atomic_Int* s, k_atomic_IntType val)
{
    return InterlockedAddNoFence(&s->volNum, -val) + val;
}

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntSubRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    return InterlockedAddRelease(&s->volNum, -val) + val;
}

K_ALWAYS_INLINE static bool
k_atomic_IntCASRelaxed(k_atomic_Int* s, k_atomic_IntType* pExpected, k_atomic_IntType desired)
{
    k_atomic_IntType r = InterlockedCompareExchangeNoFence(&s->volNum, desired, *pExpected);
    if (r == *pExpected)
    {
        return true;
    }
    else
    {
        *pExpected = r;
        return false;
    }
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64LoadRelaxed(const k_atomic_U64* s)
{
    return InterlockedCompareExchangeNoFence64((volatile __int64*)&s->volNum, 0, 0);
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64LoadAcquire(k_atomic_U64* s)
{
    return InterlockedCompareExchangeAcquire64((volatile __int64*)&s->volNum, 0, 0);
}

K_ALWAYS_INLINE static void
k_atomic_U64StoreRelaxed(k_atomic_U64* s, uint64_t val)
{
    InterlockedExchangeNoFence64((volatile __int64*)&s->volNum, val);
}

K_ALWAYS_INLINE static void
k_atomic_U64StoreRelease(k_atomic_U64* s, uint64_t val)
{
    InterlockedExchange64((volatile __int64*)&s->volNum, val);
}

K_ALWAYS_INLINE static bool
k_atomic_U64CASRelaxed(k_atomic_U64* s, uint64_t* pExpected, uint64_t desired)
{
    uint64_t r = InterlockedCompareExchangeNoFence64((volatile __int64*)&s->volNum, desired, *pExpected);
    if (r == *pExpected)
    {
        return true;
    }
    else
    {
        *pExpected = r;
        return false;
    }
}

K_ALWAYS_INLINE static bool
k_atomic_U64CASRelease(k_atomic_U64* s, uint64_t* pExpected, uint64_t desired)
{
    uint64_t r = InterlockedCompareExchangeRelease64((volatile __int64*)&s->volNum, desired, *pExpected);
    if (r == *pExpected)
    {
        return true;
    }
    else
    {
        *pExpected = r;
        return false;
    }
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64AddRelaxed(k_atomic_U64* s, uint64_t val)
{
    return InterlockedAddNoFence64((volatile __int64*)&s->volNum, val) - val;
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64AddRelease(k_atomic_U64* s, uint64_t val)
{
    return InterlockedAddRelease64((volatile __int64*)&s->volNum, val) - val;
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64SubRelaxed(k_atomic_U64* s, uint64_t val)
{
    return InterlockedAddNoFence64((volatile __int64*)&s->volNum, -val) + val;
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64SubRelease(k_atomic_U64* s, uint64_t val)
{
    return InterlockedAddRelease64((volatile __int64*)&s->volNum, -val) + val;
}

K_ALWAYS_INLINE static uint8_t
k_atomic_U8LoadRelaxed(const k_atomic_U8* s)
{
    return _InterlockedCompareExchange8((volatile char*)&s->volNum, 0, 0);
}

K_ALWAYS_INLINE static void
k_atomic_U8StoreRelaxed(k_atomic_U8* s, uint8_t val)
{
    InterlockedExchange8((volatile char*)&s->volNum, val);
}

K_ALWAYS_INLINE static void
k_atomic_U8StoreRelease(k_atomic_U8* s, uint8_t val)
{
    InterlockedExchange8((volatile char*)&s->volNum, val);
}

K_ALWAYS_INLINE static bool
k_atomic_U8CASAcquire(k_atomic_U8* s, uint8_t* pExpected, uint8_t desired)
{
    uint8_t r = _InterlockedCompareExchange8((volatile char*)&s->volNum, desired, *pExpected);
    if (r == *pExpected)
    {
        return true;
    }
    else
    {
        *pExpected = r;
        return false;
    }
}

K_ALWAYS_INLINE static bool
k_atomic_U8CASRelaxed(k_atomic_U8* s, uint8_t* pExpected, uint8_t desired)
{
    uint8_t r = _InterlockedCompareExchange8((volatile char*)&s->volNum, desired, *pExpected);
    if (r == *pExpected)
    {
        return true;
    }
    else
    {
        *pExpected = r;
        return false;
    }
}

#elif defined __unix__

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntLoadRelaxed(k_atomic_Int* s)
{
    return __atomic_load_n(&s->volNum, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntLoadAcquire(k_atomic_Int* s)
{
    return __atomic_load_n(&s->volNum, __ATOMIC_ACQUIRE);
}

K_ALWAYS_INLINE static void
k_atomic_IntStoreRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    __atomic_store_n(&s->volNum, val, __ATOMIC_RELEASE);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntAddRelaxed(k_atomic_Int* s, k_atomic_IntType val)
{
    return __atomic_fetch_add(&s->volNum, val, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntSubRelaxed(k_atomic_Int* s, k_atomic_IntType val)
{
    return __atomic_fetch_sub(&s->volNum, val, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntAddRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    return __atomic_fetch_add(&s->volNum, val, __ATOMIC_RELEASE);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_atomic_IntSubRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    return __atomic_fetch_sub(&s->volNum, val, __ATOMIC_RELEASE);
}

K_ALWAYS_INLINE static bool
k_atomic_IntCASRelaxed(k_atomic_Int* s, k_atomic_IntType* pExpected, k_atomic_IntType desired)
{
    return __atomic_compare_exchange_n(&s->volNum, pExpected, desired, false /* weak */, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64LoadRelaxed(const k_atomic_U64* s)
{
    return __atomic_load_n(&s->volNum, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64LoadAcquire(k_atomic_U64* s)
{
    return __atomic_load_n(&s->volNum, __ATOMIC_ACQUIRE);
}

K_ALWAYS_INLINE static void
k_atomic_U64StoreRelaxed(k_atomic_U64* s, uint64_t val)
{
    __atomic_store_n(&s->volNum, val, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static void
k_atomic_U64StoreRelease(k_atomic_U64* s, uint64_t val)
{
    __atomic_store_n(&s->volNum, val, __ATOMIC_RELEASE);
}

K_ALWAYS_INLINE static bool
k_atomic_U64CASRelaxed(k_atomic_U64* s, uint64_t* pExpected, uint64_t desired)
{
    return __atomic_compare_exchange_n(&s->volNum, pExpected, desired, false /* weak */, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static bool
k_atomic_U64CASRelease(k_atomic_U64* s, uint64_t* pExpected, uint64_t desired)
{
    return __atomic_compare_exchange_n(&s->volNum, pExpected, desired, false /* weak */, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64AddRelaxed(k_atomic_U64* s, uint64_t val)
{
    return __atomic_fetch_add(&s->volNum, val, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64AddRelease(k_atomic_U64* s, uint64_t val)
{
    return __atomic_fetch_add(&s->volNum, val, __ATOMIC_RELEASE);
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64SubRelaxed(k_atomic_U64* s, uint64_t val)
{
    return __atomic_fetch_sub(&s->volNum, val, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static uint64_t
k_atomic_U64SubRelease(k_atomic_U64* s, uint64_t val)
{
    return __atomic_fetch_sub(&s->volNum, val, __ATOMIC_RELEASE);
}

K_ALWAYS_INLINE static uint8_t
k_atomic_U8LoadRelaxed(const k_atomic_U8* s)
{
    return __atomic_load_n(&s->volNum, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static void
k_atomic_U8StoreRelaxed(k_atomic_U8* s, uint8_t val)
{
    __atomic_store_n(&s->volNum, val, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static void
k_atomic_U8StoreRelease(k_atomic_U8* s, uint8_t val)
{
    __atomic_store_n(&s->volNum, val, __ATOMIC_RELEASE);
}

K_ALWAYS_INLINE static bool
k_atomic_U8CASAcquire(k_atomic_U8* s, uint8_t* pExpected, uint8_t desired)
{
    return __atomic_compare_exchange_n(&s->volNum, pExpected, desired, false /* weak */, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static bool
k_atomic_U8CASRelaxed(k_atomic_U8* s, uint8_t* pExpected, uint8_t desired)
{
    return __atomic_compare_exchange_n(&s->volNum, pExpected, desired, false /* weak */, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

#endif

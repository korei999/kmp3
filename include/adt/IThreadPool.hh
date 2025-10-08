#pragma once

#include "FuncBuffer.hh"
#include "Thread.hh"
#include "atomic.hh"

namespace adt
{

struct IArena;

struct IThreadPool
{
#if !defined ADT_THREAD_POOL_ARENA_TYPE
    using ArenaType = IArena;
#else
    using ArenaType = ADT_THREAD_POOL_ARENA_TYPE;
#endif

    /* TODO: buffer size can be moved to implementation. */
    using Task = FuncBuffer<void, 56>;
    static_assert(sizeof(Task) == 64);

    template<typename T>
    struct Future : adt::Future<T>
    {
        using Base = adt::Future<T>;

        /* */

        IThreadPool* m_pPool {};

        /* */

        Future() noexcept = default;
        Future(IThreadPool* pPool) noexcept;

        /* */

        void wait() noexcept;
        decltype(auto) waitData() noexcept; /* decltype(auto) for the <void> case. */
    };

    static inline IThreadPool* g_pThreadPool;

    /* */

    static void setGlobal(IThreadPool* pThreadPool) noexcept { g_pThreadPool = pThreadPool; }
    static IThreadPool* inst() noexcept { return g_pThreadPool; }

    static int optimalThreadCount() noexcept;

    /* */

    virtual const atomic::Int& nActiveTasks() const noexcept = 0;

    virtual int nThreads() const noexcept = 0;

    virtual bool addTask(void (*pfn)(void*), void* pArg, isize argSize) noexcept = 0;

    virtual void wait(bool bHelp) noexcept = 0; /* bHelp: try to call leftover tasks on waiting thread. */

    virtual Task tryStealTask() noexcept = 0;

    virtual usize threadId() noexcept = 0;

    virtual ArenaType* createArenaForThisThread(isize reserve) noexcept = 0;
    virtual void destroyArenaForThisThread() noexcept = 0;
    virtual ArenaType* arena() noexcept = 0;

    template<typename CL>
    bool
    add(const CL& cl) noexcept
    {
        return addTask([](void* p) {
            static_cast<CL*>(p)->operator()();
        }, (void*)&cl, sizeof(cl));
    }

    template<typename T, typename CL>
    bool
    add(Future<T>* pFut, const CL& cl) noexcept
    {
        auto cl2 = [pFut, cl]
        {
            if constexpr (std::is_same_v<void, T>)
            {
                cl();
                pFut->signal();
            }
            else
            {
                pFut->signalData(cl());
            }
        };

        static_assert(sizeof(cl2) >= sizeof(cl) + sizeof(pFut));

        return addTask([](void* p) {
            static_cast<decltype(cl2)*>(p)->operator()();
        }, &cl2, sizeof(cl2));
    }

    template<typename CL>
    void addRetry(const CL& cl) noexcept { while (!add(cl)); }

    template<typename CL>
    bool
    addRetry(const CL& cl, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            if (add(cl)) return true;
        return false;
    }

    template<typename T, typename CL>
    void addRetry(Future<T>* pFut, const CL& cl) noexcept { while (!add(pFut, cl)); }

    template<typename T, typename CL>
    bool
    addRetry(Future<T>* pFut, const CL& cl, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            if (add(pFut, cl)) return true;
        return false;
    }

    template<typename CL>
    void
    addRetryOrDo(const CL& cl) noexcept
    {
        if (nActiveTasks().load(atomic::ORDER::ACQUIRE) >= nThreads()) cl();
        else addRetry(cl);
    }

    template<typename CL>
    bool
    addRetryOrDo(const CL& cl, int n) noexcept
    {
        if (nActiveTasks().load(atomic::ORDER::ACQUIRE) >= nThreads())
        {
            cl();
            return true;
        }
        else
        {
            return addRetry(cl, n);
        }
    }

    template<typename T, typename CL>
    void
    addRetryOrDo(Future<T>* pFut, const CL& cl) noexcept
    {
        if (nActiveTasks().load(atomic::ORDER::ACQUIRE) >= nThreads()) cl();
        else addRetry(pFut, cl);
    }

    template<typename T, typename CL>
    bool
    addRetryOrDo(Future<T>* pFut, const CL& cl, int n) noexcept
    {
        if (nActiveTasks().load(atomic::ORDER::ACQUIRE) >= nThreads())
        {
            cl();
            return true;
        }
        else
        {
            return addRetry(pFut, cl, n);
        }
    }
};

} /* namespace adt */

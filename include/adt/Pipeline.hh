#pragma once

#include "Thread.hh"
#include "Queue.hh"
#include "atomic.hh"

namespace adt
{

struct Pipeline
{
    struct Stage
    {
        IAllocator* pAlloc {};
        void (*pfn)(void*) {};
        Queue<void*> qInput {};
        Stage* pNextStage {};
        Pipeline* pThisPipeline {};
        CndVar cnd {};
        Mutex mtx {};
        Thread thrd {};
        i64 stageId {};

        /* */

        Stage() = default;

        Stage(IAllocator* _pAlloc, void (*_pfn)(void*))
            : pAlloc {_pAlloc},
              pfn {_pfn},
              cnd(INIT),
              mtx(INIT) {}
    };

    /* */

    Stage* m_pHead {};
    Mutex m_mtxWait {};
    CndVar m_cndWait {};
    atomic::Int m_atomBDone {};
    atomic::Int m_atomNEnqueued {};

    /* */

    Pipeline() = default;

    Pipeline(IAllocator* p, std::initializer_list<Stage> stages);

    /* */

    void add(void* pInput);
    void destroy(IAllocator* pAlloc) noexcept;
    void wait();

protected:
    static THREAD_STATUS stageLoop(void* pStage);
};

inline
Pipeline::Pipeline(IAllocator* p, std::initializer_list<Stage> stages)
    : m_mtxWait {INIT}, m_cndWait {INIT}
{
    Stage** ppCurrNewStage = &m_pHead;
    i64 i = 0;

    for (auto& stage : stages)
    {
        ADT_ASSERT(stage.pAlloc != nullptr && stage.pfn != nullptr,
            "pAlloc: {}, pfn: {}", stage.pAlloc, stage.pfn
        );

        *ppCurrNewStage = p->alloc<Stage>(stage);
        (*ppCurrNewStage)->stageId = i;
        (*ppCurrNewStage)->pThisPipeline = this;

        new(&(*ppCurrNewStage)->thrd) Thread {Pipeline::stageLoop, *ppCurrNewStage};

        ppCurrNewStage = &(*ppCurrNewStage)->pNextStage;
        ++i;
    }
}

inline void
Pipeline::add(void* pInput)
{
    ADT_ASSERT(pInput != nullptr, "");
    ADT_ASSERT(m_pHead != nullptr, "");

    m_atomNEnqueued.fetchAdd(1, atomic::ORDER::RELAXED);
    {
        LockGuard lock {&m_pHead->mtx};
        m_pHead->qInput.emplaceBack(m_pHead->pAlloc, pInput);
    }
    m_pHead->cnd.signal();
}

inline void
Pipeline::destroy(IAllocator* pAlloc) noexcept
{
    wait();
    m_atomBDone.store(true, atomic::ORDER::RELEASE);

    Stage* pStage = m_pHead;
    while (pStage)
    {
        pStage->cnd.signal();
        pStage->thrd.join();
        pStage->cnd.destroy();
        pStage->mtx.destroy();
        pStage->qInput.destroy(pStage->pAlloc);

        Stage* pNext = pStage->pNextStage;
        pAlloc->dealloc(pStage);

        pStage = pNext;
    }

    m_mtxWait.destroy();
    m_cndWait.destroy();
}

inline void
Pipeline::wait()
{
    LockGuard lock {&m_mtxWait};

    while (m_atomNEnqueued.load(atomic::ORDER::ACQUIRE) > 0)
        m_cndWait.wait(&m_mtxWait);
}

inline THREAD_STATUS
Pipeline::stageLoop(void* pStage)
{
    auto& stage = *static_cast<Stage*>(pStage);
    auto& pipeline = *stage.pThisPipeline;
    auto& atomBDone = pipeline.m_atomBDone;

    defer( print::err("[Pipeline]: stage({}) done\n", stage.stageId) );

    while (true)
    {
        void* pPackage {};

        {
            LockGuard inputLock {&stage.mtx};

            while (stage.qInput.empty() && !atomBDone.load(atomic::ORDER::ACQUIRE))
                stage.cnd.wait(&stage.mtx);

            if (atomBDone.load(atomic::ORDER::ACQUIRE)) return 0;

            pPackage = stage.qInput.popFront();
        }

        ADT_ASSERT(pPackage != nullptr, "");
        stage.pfn(pPackage);

        if (stage.pNextStage)
        {
            try
            {
                {
                    LockGuard outputLock {&stage.pNextStage->mtx};
                    stage.pNextStage->qInput.emplaceBack(stage.pNextStage->pAlloc, pPackage);
                }
                stage.pNextStage->cnd.signal();
            }
            catch (const AllocException& ex)
            {
                ex.printErrorMsg(stderr);
            }
        }
        else
        {
            pipeline.m_atomNEnqueued.fetchSub(1, atomic::ORDER::RELAXED);
            if (pipeline.m_atomNEnqueued.load(atomic::ORDER::ACQUIRE) <= 0)
                pipeline.m_cndWait.signal();
        }
    }

    return THREAD_STATUS(0);
}

} /* namespace adt */

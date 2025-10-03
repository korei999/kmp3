#pragma once

#include "SList.hh"

namespace adt
{

template<typename ALLOC_T>
struct ArenaDeleterCRTP
{
    using PfnDeleter = void(*)(void**);

    struct DeleterNode
    {
        void** ppObj {};
        PfnDeleter pfnDelete {};
    };

    template<typename T>
    struct Ptr : protected SList<DeleterNode>::Node
    {
        using ListNode = SList<DeleterNode>::Node;

        /* */

        T* m_pData {};

        /* */

        Ptr() noexcept = default;

        template<typename ...ARGS>
        Ptr(ALLOC_T* pArena, ARGS&&... args)
            : ListNode{nullptr, {(void**)this, (PfnDeleter)nullptrDeleter}},
              m_pData{pArena->template alloc<T>(std::forward<ARGS>(args)...)}
        {
            pArena->m_pLCurrentDeleters->insert(static_cast<ListNode*>(this));
        }

        template<typename ...ARGS>
        Ptr(void (*pfn)(Ptr*), ALLOC_T* pArena, ARGS&&... args)
            : ListNode{nullptr, {(void**)this, (PfnDeleter)pfn}},
              m_pData{pArena->template alloc<T>(std::forward<ARGS>(args)...)}
        {
            pArena->m_pLCurrentDeleters->insert(static_cast<ListNode*>(this));
        }

        /* */

        static void
        nullptrDeleter(Ptr* pPtr) noexcept
        {
            utils::destruct(pPtr->m_pData);
            pPtr->m_pData = nullptr;
        };

        static void simpleDeleter(Ptr* pPtr) noexcept { utils::destruct(pPtr->m_pData); };

        /* */

        explicit operator bool() const noexcept { return m_pData != nullptr; }

        T& operator*() noexcept { ADT_ASSERT(m_pData != nullptr, ""); return *m_pData; }
        const T& operator*() const noexcept { ADT_ASSERT(m_pData != nullptr, ""); return *m_pData; }

        T* operator->() noexcept { ADT_ASSERT(m_pData != nullptr, ""); return m_pData; }
        const T* operator->() const noexcept { ADT_ASSERT(m_pData != nullptr, ""); return m_pData; }
    };

    /* */

    SList<DeleterNode> m_lDeleters {}; /* Run deleters on reset()/freeAll() or state restorations. */
    SList<DeleterNode>* m_pLCurrentDeleters {}; /* Switch and restore current list on scope changes. */

    /* */

    ArenaDeleterCRTP() = default;
    ArenaDeleterCRTP(InitFlag) noexcept : m_pLCurrentDeleters{&m_lDeleters} {}

    /* */

    /* BUG: asan sees it as stack-use-after-scope when running a deleter after variable's scope closes (its fine just ignore). */
    ADT_NO_UB void
    runDeleters() noexcept
    {
        for (auto e : *m_pLCurrentDeleters)
            e.pfnDelete(e.ppObj);

        m_pLCurrentDeleters->m_pHead = nullptr;
    }
};

} /* namespace adt */

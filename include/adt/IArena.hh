#pragma once

#include "SList.hh"

namespace adt
{

struct IArena : IAllocator
{
    /* Used to capture (or partially capture) state of the arena to restore it later.
     * Works very well for temporary allocations. */
    struct IScopeDestructor
    {
        virtual ~IScopeDestructor() noexcept = 0;
    };

    struct IScope
    {
        IScope(IScopeDestructor* p) noexcept : m_pScope{p} {}

        ~IScope() noexcept { m_pScope->~IScopeDestructor(); }

    private:
        IScopeDestructor* m_pScope {};
    };

    /* Faster (non virtual) version. */
    template<typename ARENA_T>
    struct Scope
    {
        static_assert(false, "no overload found");
        Scope(ARENA_T* pArena);
    };

    /* */

    virtual constexpr void freeAll() noexcept = 0;
    [[nodiscard]] virtual IScope restoreAfterScope() noexcept = 0;
    usize virtual memoryUsed() const noexcept = 0;

    /* */

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
        Ptr(IArena* pArena, ARGS&&... args)
            : ListNode{nullptr, {(void**)this, (PfnDeleter)nullptrDeleter}},
              m_pData{pArena->template alloc<T>(std::forward<ARGS>(args)...)}
        {
            ADT_ASSERT(pArena->m_pLCurrentDeleters != nullptr, "");
            pArena->m_pLCurrentDeleters->insert(static_cast<ListNode*>(this));
        }

        template<typename ...ARGS>
        Ptr(void (*pfn)(Ptr*), IArena* pArena, ARGS&&... args)
            : ListNode{nullptr, {(void**)this, (PfnDeleter)pfn}},
              m_pData{pArena->template alloc<T>(std::forward<ARGS>(args)...)}
        {
            ADT_ASSERT(pArena->m_pLCurrentDeleters != nullptr, "");
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

    IArena() = default;
    IArena(InitFlag) noexcept : m_pLCurrentDeleters{&m_lDeleters} {}

    /* BUG: asan sees it as stack-use-after-scope when running a deleter after variable's scope closes (its fine just ignore). */
    ADT_NO_UB void
    runDeleters() noexcept
    {
        for (auto e : *m_pLCurrentDeleters)
            e.pfnDelete(e.ppObj);

        m_pLCurrentDeleters->m_pHead = nullptr;
    }
};

template<>
struct IArena::Scope<IArena>
{
    Scope(IArena* pArena) : m_scope{pArena->restoreAfterScope()} {}

private:
    IScope m_scope;
};

inline IArena::IScopeDestructor::~IScopeDestructor() noexcept {}

namespace print
{

template<typename T>
inline isize
format(Context* ctx, FormatArgs fmtArgs, const IArena::Ptr<T>& x)
{
    if (x) return format(ctx, fmtArgs, *x);
    else return format(ctx, fmtArgs, "null");
}

} /* namespace print */

} /* namespace adt */

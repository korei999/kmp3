#pragma once

#include "assert.hh"
#include "Gpa.hh"
#include "types.hh"

namespace adt
{

namespace details
{

struct IRefCounter
{
    i32 m_count;
    i32 m_weakCount;

    /* */

    IRefCounter() noexcept : m_count(1), m_weakCount(1) {}

    /* */

    virtual void destroyData() noexcept = 0;
    virtual void destroy() noexcept = 0;

    /* */

    void ref() noexcept;
    void weakRef() noexcept;
    void unref();
    void weakUnref();
};

inline void
IRefCounter::ref() noexcept
{
    ++m_count, ++m_weakCount;
}

inline void
IRefCounter::weakRef() noexcept
{
    ++m_weakCount;
}

inline void
IRefCounter::unref()
{
    ADT_ASSERT(m_count > 0, "{}", m_count);
    if (--m_count <= 0)
        destroyData();

    weakUnref();
}

inline void
IRefCounter::weakUnref()
{
    ADT_ASSERT(m_weakCount > 0, "{}", m_weakCount);
    if (--m_weakCount <= 0)
        destroy();
}

template<typename T>
struct RefCounterOneAlloc : IRefCounter
{
    using IRefCounter::IRefCounter;

    /* */

    virtual void
    destroyData() noexcept
    {
        T* pData = reinterpret_cast<T*>((u8*)this + sizeof(RefCounterOneAlloc));
        utils::destruct(pData);
    }

    virtual void
    destroy() noexcept
    {
        Gpa::inst()->free(this);
    }
};

template<typename T, typename CL_DELETER>
struct RefCounterOneAllocCustom : RefCounterOneAlloc<T>
{
    ADT_NO_UNIQUE_ADDRESS CL_DELETER m_clDeleter;

    /* */

    using Base = RefCounterOneAlloc<T>;

    RefCounterOneAllocCustom(CL_DELETER clDeleter) : m_clDeleter {clDeleter} {}

    /* */

    virtual void
    destroyData() noexcept
    {
        auto* pData = reinterpret_cast<T*>((u8*)this + sizeof(RefCounterOneAllocCustom));
        m_clDeleter(pData);
    }

    virtual void
    destroy() noexcept
    {
        Gpa::inst()->free(this);
    }
};

template<typename T>
struct RefCounterTwoAllocs : details::IRefCounter
{
    using Base = details::IRefCounter;

    /* */

    T* m_ptr;

    /* */

    using Base::Base;

    RefCounterTwoAllocs(T* p) : Base(), m_ptr {p} {}

    /* */

    virtual void
    destroyData() noexcept
    {
        Gpa::inst()->dealloc(m_ptr);
    }

    virtual void
    destroy() noexcept
    {
        Gpa::inst()->dealloc(this);
    }
};

template<typename T, typename CL_DELETER>
struct RefCounterTwoAllocsCustom : RefCounterTwoAllocs<T>
{
    using Base = RefCounterTwoAllocs<T>;

    /* */

    T* m_ptr;
    ADT_NO_UNIQUE_ADDRESS CL_DELETER m_clDeleter;

    /* */

    using Base::Base;
    RefCounterTwoAllocsCustom(T* ptr, CL_DELETER clDeleter) : m_ptr {ptr}, m_clDeleter {clDeleter} {}

    /* */

    virtual void
    destroyData() noexcept
    {
        m_clDeleter(m_ptr);
    }

    virtual void
    destroy() noexcept
    {
        Gpa::inst()->dealloc(this);
    }

};

} /* namespace details */

template<typename T> struct WeakPtr;

template<typename T>
struct RefCountedPtr
{
    details::IRefCounter* m_pRC;
    T* m_pData;

    /* */

    RefCountedPtr() noexcept : m_pRC {nullptr}, m_pData {nullptr} {}
    RefCountedPtr(UninitFlag) noexcept {}
    RefCountedPtr(T* ptr);

    template<typename CL_DELETER>
    RefCountedPtr(CL_DELETER clDeleter, T* ptr);

    /* */

    template<typename ...ARGS>
    [[nodiscard]] static RefCountedPtr alloc(ARGS&&... args);

    template<typename CL_DELETER, typename ...ARGS>
    [[nodiscard]] static RefCountedPtr allocWithDeleter(CL_DELETER clDeleter, ARGS&&... args);

    /* */

    operator bool() const noexcept { return m_pRC && m_pRC->m_count > 0; }

    T* operator->() noexcept { ADT_ASSERT(bool(*this), ""); return m_pData; }
    const T* operator->() const noexcept { ADT_ASSERT(bool(*this), ""); return m_pData; }
    T& operator*() noexcept { ADT_ASSERT(bool(*this), ""); return *m_pData; }
    const T& operator*() const noexcept { ADT_ASSERT(bool(*this), ""); return *m_pData; }

    T* ptr() noexcept { ADT_ASSERT(bool(*this), ""); return m_pData; }
    const T* ptr() const noexcept { ADT_ASSERT(bool(*this), ""); return m_pData; }

    RefCountedPtr& ref() noexcept;
    WeakPtr<T> weakRef() noexcept;
    void unref();
    void weakUnref();
};

template<typename T>
struct WeakPtr : RefCountedPtr<T>
{
    using Base = RefCountedPtr<T>;

    /* */

    WeakPtr() : Base {} {}
    WeakPtr(Base rcp) : Base {rcp} {}

    /* */

    void ref() = delete;
    void unref() = delete;
};

template<typename T>
inline
RefCountedPtr<T>::RefCountedPtr(T* ptr)
    : m_pRC {Gpa::inst()->alloc<details::RefCounterTwoAllocs<T>>(ptr)}, m_pData {ptr}
{
}

template<typename T>
template<typename CL_DELETER>
inline
RefCountedPtr<T>::RefCountedPtr(CL_DELETER clDeleter, T* ptr)
    : m_pRC {Gpa::inst()->alloc<details::RefCounterTwoAllocsCustom<T, CL_DELETER>>(ptr, clDeleter)},
      m_pData {ptr}
{
}

template<typename T>
template<typename ...ARGS>
inline RefCountedPtr<T>
RefCountedPtr<T>::alloc(ARGS&&... args)
{
    using DeleterType = details::RefCounterOneAlloc<T>;

    void* pBoth = Gpa::inst()->malloc(sizeof(DeleterType) + sizeof(T));

    RefCountedPtr rcp {UNINIT};
    rcp.m_pRC = static_cast<DeleterType*>(pBoth);
    rcp.m_pData = reinterpret_cast<T*>((u8*)pBoth + sizeof(DeleterType));

    new(rcp.m_pRC) DeleterType ();
    new(rcp.m_pData) T (std::forward<ARGS>(args)...);

    return rcp;
}

template<typename T>
template<typename CL_DELETER, typename ...ARGS>
inline RefCountedPtr<T>
RefCountedPtr<T>::allocWithDeleter(CL_DELETER clDeleter, ARGS&&... args)
{
    using DeleterType = details::RefCounterOneAllocCustom<T, CL_DELETER>;

    void* pBoth = Gpa::inst()->malloc(sizeof(DeleterType) + sizeof(T));

    RefCountedPtr rcp {UNINIT};
    rcp.m_pRC = static_cast<DeleterType*>(pBoth);
    rcp.m_pData = reinterpret_cast<T*>((u8*)pBoth + sizeof(DeleterType));

    new(rcp.m_pRC) DeleterType (clDeleter);
    new(rcp.m_pData) T (std::forward<ARGS>(args)...);

    return rcp;
}

template<typename T>
inline RefCountedPtr<T>&
RefCountedPtr<T>::ref() noexcept
{
    ADT_ASSERT(m_pRC != nullptr, "");
    m_pRC->ref();
    return *this;
}

template<typename T>
inline WeakPtr<T>
RefCountedPtr<T>::weakRef() noexcept
{
    ADT_ASSERT(m_pRC != nullptr, "");
    m_pRC->weakRef();
    return *this;
}

template<typename T>
inline void
RefCountedPtr<T>::unref()
{
    ADT_ASSERT(m_pRC != nullptr, "");
    m_pRC->unref();
}

template<typename T>
inline void
RefCountedPtr<T>::weakUnref()
{
    ADT_ASSERT(m_pRC != nullptr, "");
    m_pRC->weakUnref();
}

template<typename T>
struct RefScope
{
    RefScope(RefCountedPtr<T>* p) noexcept
    {
        ADT_ASSERT(p != nullptr, "");
        ADT_ASSERT(bool(*p), "count: {}, weakCount: {}", p->m_pRC->m_count, p->m_pRC->m_weakCount);
        m_pRC = &p->ref();
    }

    ~RefScope() { m_pRC->unref(); }

protected:
    RefCountedPtr<T>* m_pRC;
};

template<typename T>
struct WeakRefScope
{
    WeakRefScope(WeakPtr<T>* p) noexcept
    {
        ADT_ASSERT(p != nullptr, "");
        ADT_ASSERT(bool(*p), "count: {}, weakCount: {}", p->m_pRC->m_count, p->m_pRC->m_weakCount);
        m_pRC = &p->ref();
    }

    ~WeakRefScope() { m_pRC->weakUnref(); }

protected:
    WeakPtr<T>* m_pRC;
};

} /* namespace adt */

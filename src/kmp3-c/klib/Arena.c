#include "Arena.h"

#include <assert.h>
#include <stdlib.h>

#if defined __unix__
    #define K_ARENA_MMAP
    #include <sys/mman.h>
#elif defined _WIN32
    #define K_ARENA_WIN32

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #include <windows.h>
#else
    #error "Arena is not implemented"
#endif

static bool
commit(void* p, ssize_t size)
{
#ifdef K_ARENA_MMAP

    int err = mprotect(p, size, PROT_READ | PROT_WRITE);
    if (err == -1) return false;

#elif defined K_ARENA_WIN32

    if (!VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE))
        return false;

#else
#endif

    return true;
}

static void
decommit(void* p, ssize_t size)
{
#ifdef K_ARENA_MMAP

    int err = mprotect(p, size, PROT_NONE);
    if K_UNLIKELY(err == - 1) goto fail;

    err = madvise(p, size, MADV_DONTNEED);
    if K_UNLIKELY(err == - 1) goto fail;

#elif defined K_ARENA_WIN32

    if K_UNLIKELY(!VirtualFree(p, size, MEM_DECOMMIT))
        goto fail;

#else
#endif

    return;

fail:
    assert(false && "very unexpected decommit failure");
}

static bool
growIfNeeded(k_Arena* pSelf, ssize_t newPos)
{
    K_TYPEOF(pSelf->priv)* s = &pSelf->priv;

    if (newPos > s->commited)
    {
        const ssize_t pageSize = k_getPageSize();
        ssize_t aligned = K_ALIGN_UP_PO2(newPos, pageSize);
        const ssize_t newCommited = K_MAX(aligned, s->commited * 2);

        if K_UNLIKELY(newCommited > s->reserved)
            return false;

        if K_UNLIKELY(!commit((uint8_t*)s->pData + s->commited, newCommited - s->commited))
            return false;

        s->commited = newCommited;
    }

#if defined K_ASAN
    if (newPos > s->pos)
        K_ASAN_UNPOISON((uint8_t*)s->pData + s->pos, newPos - s->pos);
    else if (newPos < s->pos)
        K_ASAN_POISON((uint8_t*)s->pData + newPos, s->pos - newPos);
#endif

    s->pos = newPos;
    return true;
}

static inline void* ArenaMallocStub(k_IAllocator* s, ssize_t nBytes) { return k_ArenaMalloc((k_Arena*)s, nBytes); }
static inline void* ArenaZallocStub(k_IAllocator* s, ssize_t nBytes) { return k_ArenaZalloc((k_Arena*)s, nBytes); }
static inline void* ArenaReallocStub(k_IAllocator* s, void* p, ssize_t nBytesOld, ssize_t nBytesNew) { return k_ArenaRealloc((k_Arena*)s, p, nBytesOld, nBytesNew); }
static inline void ArenaFreeStub(k_IAllocator* s, void* p) { k_ArenaFree((k_Arena*)s, p); }

bool
k_ArenaInit(k_Arena* s, ssize_t reserveSize, ssize_t commitSize)
{
    static const k_IAllocatorVTable s_vtArena = {
        .malloc = ArenaMallocStub,
        .zalloc = ArenaZallocStub,
        .realloc = ArenaReallocStub,
        .free = ArenaFreeStub,
    };
    s->base.pVTable = &s_vtArena;
    s->priv.pData = NULL;

    int err = 0;
    (void)err;

    assert(reserveSize > 0);

    const ssize_t pageSize = k_getPageSize();
    const ssize_t realReserved = K_ALIGN_UP_PO2(reserveSize, pageSize);

#ifdef K_ARENA_MMAP
    void* pRes = mmap(NULL, realReserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pRes == MAP_FAILED) return false;
#elif defined K_ARENA_WIN32
    void* pRes = VirtualAlloc(NULL, realReserved, MEM_RESERVE, PAGE_READWRITE);
    if (!pRes) return false;
#else
#endif

    s->priv.pData = pRes;
    s->priv.pos = 0;
    s->priv.reserved = realReserved;
    s->priv.commited = 0;
    s->priv.pLastAlloc = (void*)K_NPOS64;
    s->priv.lDeleters = NULL;
    s->priv.pLCurrentDeleters = &s->priv.lDeleters;

    K_ASAN_POISON(s->priv.pData, realReserved);

    if (commitSize > 0)
    {
        const ssize_t realCommit = K_ALIGN_UP_PO2(commitSize, pageSize);
        if K_UNLIKELY(!commit(s->priv.pData, realCommit))
            return false;
        s->priv.commited = realCommit;
    }

    return true;
}

void*
k_ArenaMalloc(k_Arena* s, ssize_t nBytes)
{
    const ssize_t realSize = K_ALIGN_UP8(nBytes);
    void* pRet = (void*)((uint8_t*)s->priv.pData + s->priv.pos);
    if K_UNLIKELY(!growIfNeeded(s, s->priv.pos + realSize)) return NULL;
    s->priv.pLastAlloc = pRet;

    return pRet;
}

void*
k_ArenaZalloc(k_Arena* s, ssize_t nBytes)
{
    void* pMem = k_ArenaMalloc(s, nBytes);
    memset(pMem, 0, nBytes);
    return pMem;
}

void*
k_ArenaRealloc(k_Arena* s, void* p, ssize_t oldNBytes, ssize_t newNBytes)
{
    if (!p) return k_ArenaMalloc(s, newNBytes);

    /* bump case */
    if (p == s->priv.pLastAlloc)
    {
        const ssize_t realSize = K_ALIGN_UP8(newNBytes);
        const ssize_t newPos = ((ssize_t)s->priv.pLastAlloc - (ssize_t)s->priv.pData) + realSize;
        if K_UNLIKELY(!growIfNeeded(s, newPos)) return NULL;
        return p;
    }

    if (newNBytes <= oldNBytes) return p;

    void* pMem = k_ArenaMalloc(s, newNBytes);
    if (p) memcpy(pMem, p, K_MIN(oldNBytes, newNBytes));
    return pMem;
}

void
k_ArenaDestroy(k_Arena* s)
{
    k_ArenaRunDeleters(s);

    if (s->priv.pData)
    {
#ifdef K_ARENA_MMAP
        int err = munmap(s->priv.pData, s->priv.reserved);
        (void)err;
        assert(err != - 1);
#elif defined K_ARENA_WIN32
        VirtualFree(s->priv.pData, 0, MEM_RELEASE);
#else
#endif
        K_ASAN_UNPOISON(s->priv.pData, s->priv.reserved);
        *s = (k_Arena){0};
    }
}

void
k_ArenaReset(k_Arena* s)
{
    k_ArenaRunDeleters(s);

    K_ASAN_POISON(s->priv.pData, s->priv.pos);

    s->priv.pos = 0;
    s->priv.pLastAlloc = (void*)K_NPOS64;
}

void
k_ArenaResetDecommit(k_Arena* s)
{
    k_ArenaRunDeleters(s);

    decommit(s->priv.pData, s->priv.commited);

    K_ASAN_POISON(s->priv.pData, s->priv.pos);

    s->priv.pos = 0;
    s->priv.commited = 0;
    s->priv.pLastAlloc = (void*)K_NPOS64;
}

void
k_ArenaResetToPage(k_Arena* pSelf, ssize_t nthPage)
{
    K_TYPEOF(pSelf->priv)* s = &pSelf->priv;

    const ssize_t commitSize = k_getPageSize() * nthPage;
    assert(commitSize <= s->reserved);

    k_ArenaRunDeleters(pSelf);

    if (s->commited > commitSize)
    {
        decommit((uint8_t*)s->pData + commitSize, s->commited - commitSize);
    }
    else if (s->commited < commitSize)
    {
        if K_UNLIKELY(!commit((uint8_t*)s->pData + s->commited, commitSize - s->commited))
            assert(false && "this commit shouldn't have failed, terminating...");
    }

    K_ASAN_POISON(s->pData, s->reserved);

    s->pos = 0;
    s->commited = commitSize;
    s->pLastAlloc = (void*)K_NPOS64;
}

void
k_ArenaRunDeleters(k_Arena* s)
{
    for (k_ArenaPtr* walk = *s->priv.pLCurrentDeleters; walk; walk = walk->pNext)
        walk->pfnDeleter(walk->ppObj);

    *s->priv.pLCurrentDeleters = NULL;
}

bool
k_ArenaPtrAlloc(k_Arena* s, k_ArenaPtrAllocOpts opts)
{
    void* pMem = k_ArenaMalloc(s, opts.objByteSize);
    if (!pMem) return false;

    *opts.ppObj = pMem;
    if (!opts.pfnDeleter) opts.pNode->pfnDeleter = k_nullDeleter;

    opts.pNode->ppObj = opts.ppObj;
    opts.pNode->pNext = *s->priv.pLCurrentDeleters;
    *s->priv.pLCurrentDeleters = opts.pNode;

    return true;
}

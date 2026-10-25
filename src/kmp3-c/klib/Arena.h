#pragma once

#include "IAllocator.h"

typedef void (*k_ArenaDeleterPfn)(void**);

typedef struct k_ArenaPtr
{
    struct k_ArenaPtr* pNext;
    void** ppObj;
    k_ArenaDeleterPfn pfnDeleter;
    /* T* pObj; */
} k_ArenaPtr;

typedef struct k_ArenaPtrAllocOpts
{
    k_ArenaPtr* pNode;
    void** ppObj;
    ssize_t objByteSize;
    k_ArenaDeleterPfn pfnDeleter; /* k_nullDeleter if NULL. */
} k_ArenaPtrAllocOpts;

typedef struct k_Arena
{
    k_IAllocator base;

    struct
    {
        void* pData;
        ssize_t pos;
        ssize_t reserved;
        ssize_t commited;
        void* pLastAlloc;
        k_ArenaPtr* lDeleters;
        k_ArenaPtr** pLCurrentDeleters;
    } priv;
} k_Arena;

bool k_ArenaInit(k_Arena* s, ssize_t reserveSize, ssize_t commitSize);
K_NO_DISCARD void* k_ArenaMalloc(k_Arena* s, ssize_t nBytes);
K_NO_DISCARD void* k_ArenaZalloc(k_Arena* s, ssize_t nBytes);
K_NO_DISCARD void* k_ArenaRealloc(k_Arena* s, void* p, ssize_t oldNBytes, ssize_t newNBytes);
static inline void k_ArenaFree(k_Arena* s, void* ptr) { (void)s, (void)ptr; /* noop */ }
void k_ArenaDestroy(k_Arena* s);
void k_ArenaReset(k_Arena* s);
void k_ArenaResetDecommit(k_Arena* s);
void k_ArenaResetToPage(k_Arena* s, ssize_t nthPage);
void k_ArenaRunDeleters(k_Arena* s);
bool k_ArenaPtrAlloc(k_Arena* s, k_ArenaPtrAllocOpts opts);
static inline ssize_t k_ArenaMemoryReserved(k_Arena* s);
static inline ssize_t k_ArenaMemoryUsed(k_Arena* s);

static inline void*
k_ArenaAlloc(k_Arena* s, void* p, ssize_t size)
{
    void* ret = k_ArenaMalloc(s, size);
    if (!ret) return NULL;
    memcpy(ret, p, size);
    return ret;
}

static inline ssize_t
k_ArenaMemoryReserved(k_Arena* s)
{
    return s->priv.reserved;
}

static inline ssize_t
k_ArenaMemoryUsed(k_Arena* s)
{
    return s->priv.pos;
}

#define K_ARENA_ALLOC(pArena, type, ...) (type*)k_ArenaAlloc(pArena, &(type) {__VA_ARGS__}, sizeof(type))

typedef struct
{
    k_Arena* pArena;
    ssize_t pos;
    void* pLastAlloc;
} k_ArenaState;

static inline k_ArenaState k_ArenaStatePush(k_Arena* pArena);
static inline void k_ArenaStateRestore(k_ArenaState* s);

typedef struct
{
    struct
    {
        k_Arena* pArena;
        ssize_t pos;
        void* pLastAlloc;
        k_ArenaPtr** pLCurrentDeleters;
    } state;
    k_ArenaPtr* lDeleters; /* New list. */
} k_ArenaState2;

/* Also push new deleters list. */
static inline void k_ArenaStatePushDeleters(k_ArenaState2* s, k_Arena* pArena);
static inline void k_ArenaStateRestoreDeleters(k_ArenaState2* s);

#define K_ARENA_SCOPE_VAR(pArena, name)                                                                                \
    for (k_ArenaState name = k_ArenaStatePush(pArena), *K_GLUE(_pState, name) = NULL; !K_GLUE(_pState, name);          \
         K_GLUE(_pState, name) = (k_ArenaStateRestore(&name), (k_ArenaState*)K_NPOS64))

#define K_ARENA_SCOPE_AUTO_VAR(pArena, name)                                                                           \
    for (k_ArenaState K_GLUE(_state__, name) = k_ArenaStatePush(pArena), *K_GLUE(_pState__, name) = NULL;              \
         !K_GLUE(_pState__, name);                                                                                     \
         K_GLUE(_pState__, name) = (k_ArenaStateRestore(&K_GLUE(_state__, name)), (k_ArenaState*)K_NPOS64))

#define K_ARENA_SCOPE_AUTO_VAR_DELETERS(pArena, name)                                                                  \
    for (k_ArenaState2 K_GLUE(_state__, name),                                                                         \
         *K_GLUE(_pState__, name) = (k_ArenaStatePushDeleters(&K_GLUE(_state__, name), pArena), NULL);                 \
         !K_GLUE(_pState__, name);                                                                                     \
         K_GLUE(_pState__, name) = (k_ArenaStateRestoreDeleters(&K_GLUE(_state__, name)), (k_ArenaState2*)K_NPOS64))

#define K_ARENA_SCOPE(pArena) K_ARENA_SCOPE_AUTO_VAR(pArena, __COUNTER__)
#define K_ARENA_SCOPE_DELETERS(pArena) K_ARENA_SCOPE_AUTO_VAR_DELETERS(pArena, __COUNTER__)

static inline k_ArenaState
k_ArenaStatePush(k_Arena* pArena)
{
    return (k_ArenaState){
        .pArena = pArena,
        .pos = pArena->priv.pos,
        .pLastAlloc = pArena->priv.pLastAlloc,
    };
}

static inline void
k_ArenaStateRestore(k_ArenaState* s)
{
    K_ASAN_POISON((uint8_t*)s->pArena->priv.pData + s->pArena->priv.pos, s->pArena->priv.pos - s->pos);
    s->pArena->priv.pos = s->pos;
    s->pArena->priv.pLastAlloc = s->pLastAlloc;
}

static inline void
k_ArenaStatePushDeleters(k_ArenaState2* s, k_Arena* pArena)
{
    s->state.pArena = pArena;
    s->state.pos = pArena->priv.pos;
    s->state.pLastAlloc = pArena->priv.pLastAlloc;
    s->state.pLCurrentDeleters = pArena->priv.pLCurrentDeleters;

    s->lDeleters = NULL;
    s->state.pArena->priv.pLCurrentDeleters = &s->lDeleters;
}

static inline void
k_ArenaStateRestoreDeleters(k_ArenaState2* s)
{
    k_ArenaRunDeleters(s->state.pArena);
    K_ASAN_POISON((uint8_t*)s->state.pArena->priv.pData + s->state.pArena->priv.pos, s->state.pArena->priv.pos - s->state.pos);
    s->state.pArena->priv.pos = s->state.pos;
    s->state.pArena->priv.pLastAlloc = s->state.pLastAlloc;
    s->state.pArena->priv.pLCurrentDeleters = s->state.pLCurrentDeleters;
}

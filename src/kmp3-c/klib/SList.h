#pragma once

typedef struct k_SListNode
{
    struct k_SListNode* pNext;
    /* <T> data; */
} k_SListNode;

typedef struct k_SList
{
    k_SListNode* pFirst;
} k_SList;

typedef struct k_SListRemoveOpts
{
    k_SListNode* pPrev;
    k_SListNode* pNode;
} k_SListRemoveOpts;

typedef struct k_SListInsertAfterOpts
{
    k_SListNode* pAfter;
    k_SListNode* pNode;
} k_SListInsertAfterOpts;

typedef struct k_SListInsertBeforeOpts
{
    k_SListNode* pBefore;
    k_SListNode* pNode;
} k_SListInsertBeforeOpts;

#define K_SLIST_FOR_EACH(s, pItName) for (k_SListNode* pItName = (s)->pFirst; pItName; pItName = pItName->pNext)
#define K_SLIST_NODE_FOR_EACH(s, pItName) for (k_SListNode* pItName = s; pItName; pItName = pItName->pNext)

static inline k_SList k_SListCreate(void);
static inline void k_SListInsert(k_SList* s, k_SListNode* pNode); /* Prepend. */
static inline void k_SListInsertTail(k_SList* s, k_SListNode* pNode); /* Append. */
static inline void k_SListInsertAfter(k_SListInsertAfterOpts opts);
static inline void k_SListInsertBefore(k_SList* s, k_SListInsertBeforeOpts opts);
static inline void k_SListRemove(k_SList* s, k_SListNode* pNode);
static inline k_SListNode* k_SListRemove1(k_SList* s, k_SListNode* pNode); /* Returns previous node or NULL. */
static inline void k_SListRemove2(k_SListRemoveOpts opts);

static inline k_SList
k_SListCreate(void)
{
    return (k_SList){0};
}

static inline void
k_SListInsert(k_SList* s, k_SListNode* pNode)
{
    pNode->pNext = s->pFirst;
    s->pFirst = pNode;
}

static inline void
k_SListInsertTail(k_SList* s, k_SListNode* pNode)
{
    k_SListNode** walk = &s->pFirst;
    while (*walk) walk = &(*walk)->pNext;
    *walk = pNode;
    pNode->pNext = (void*)0;
}

static inline void
k_SListInsertAfter(k_SListInsertAfterOpts opts)
{
    opts.pNode->pNext = opts.pAfter->pNext;
    opts.pAfter->pNext = opts.pNode;
}

static inline void
k_SListInsertBefore(k_SList* s, k_SListInsertBeforeOpts opts)
{
    if (opts.pBefore == s->pFirst)
    {
        opts.pNode->pNext = s->pFirst;
        s->pFirst = opts.pNode;
        return;
    }

    k_SListNode** walk = &s->pFirst->pNext;
    while (*walk)
    {
        if (*walk == opts.pBefore)
        {
            k_SListNode* pPrev = (k_SListNode*)walk;
            opts.pNode->pNext = pPrev->pNext;
            pPrev->pNext = opts.pNode;
            return;
        }
        walk = &(*walk)->pNext;
    }
}

static inline void
k_SListRemove(k_SList* s, k_SListNode* pNode)
{
    k_SListNode** walk = &s->pFirst;
    while (*walk)
    {
        if (*walk == pNode)
        {
            *walk = pNode->pNext;
            return;
        }
        walk = &(*walk)->pNext;
    }
}

static inline k_SListNode*
k_SListRemove1(k_SList* s, k_SListNode* pNode)
{
    k_SListNode** walk = &s->pFirst;
    while (*walk)
    {
        if (*walk == pNode)
        {
            *walk = pNode->pNext;
            return *walk == s->pFirst ? (void*)0 : (k_SListNode*)walk;
        }
        walk = &(*walk)->pNext;
    }

    return (void*)0;
}

static inline void
k_SListRemove2(k_SListRemoveOpts opts)
{
    opts.pPrev->pNext = opts.pNode->pNext;
}

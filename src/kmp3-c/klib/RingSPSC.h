#pragma once

#include "IAllocator.h"
#include "atomic.h"

typedef struct
{
    _Alignas(K_CACHE_SIZE) k_atomic_U64 atomHeadI;
    uint64_t capMinusOne;
    uint8_t* pData;
    _Alignas(K_CACHE_SIZE) k_atomic_U64 atomTailI;
} k_RingSPSC;

bool k_RingSPSCInit(k_RingSPSC* s, k_IAllocator* pAlloc, ssize_t cap); /* False on allocation failure. */
void k_RingSPSCDestroy(k_RingSPSC* s, k_IAllocator* pAlloc);
bool k_RingSPSCPush(k_RingSPSC* s, const uint8_t* p, ssize_t size, ssize_t* pSizeOrNull); /* False if atomic push failed. */
ssize_t k_RingSPSCPop(k_RingSPSC* s, uint8_t* pFill, ssize_t fillBuffSize); /* Returns n bytes written. */
ssize_t k_RingSPSCSize(k_RingSPSC* s);
ssize_t k_RingSPSCCap(k_RingSPSC* s);
void k_RingSPSCClear(k_RingSPSC* s);

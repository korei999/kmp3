#include "String.h"

K_NO_DISCARD k_String
k_StringCreateSmallSv(const k_StringView sv)
{
    k_String s;
    const ssize_t svSize = K_CLAMP(sv.size, 0, K_STRING_SMALL_SIZE - 1);
    memcpy(s.priv.aBuff, sv.pData, svSize);
    s.priv.aBuff[svSize] = '\0';
    s.priv.cap = K_STRING_SMALL_SIZE;
    return s;
}

k_String
k_StringCreate(k_IAllocator* pAlloc, const char* pData, ssize_t size)
{
    k_String s = {0};

    if (size < K_STRING_SMALL_SIZE)
    {
        memcpy(s.priv.aBuff, pData, size);
        s.priv.cap = K_STRING_SMALL_SIZE;
    }
    else
    {
        ssize_t cap = K_ALIGN_UP8(size + 1);
        char* pNew = k_IAllocatorMalloc(pAlloc, cap);
        if (!pNew) return s;

        memcpy(pNew, pData, size);
        pNew[size] = '\0';
        s.priv.ptr.pData = pNew;
        s.priv.ptr.size = size;
        s.priv.cap = cap;
    }

    return s;
}

void
k_StringDestroy(k_String* s, k_IAllocator* pAlloc)
{
    if (s->priv.cap > K_STRING_SMALL_SIZE)
    {
        k_IAllocatorFree(pAlloc, s->priv.ptr.pData);
        s->priv.cap = K_STRING_SMALL_SIZE;
    }

    s->priv.aBuff[0] = '\0';
}

bool
k_StringReallocWith(k_String* s, k_IAllocator* pAlloc, const k_StringView svWith)
{
    if (s->priv.cap <= 0) s->priv.cap = K_STRING_SMALL_SIZE;

    k_StringView svThis = k_StringToSv(s);

    if (s->priv.cap > svWith.size)
    {
        memcpy(svThis.pData, svWith.pData, svWith.size);
        svThis.pData[svWith.size] = '\0';
        if (s->priv.cap > K_STRING_SMALL_SIZE) s->priv.ptr.size = svWith.size;
        return true;
    }

    ssize_t newCap = s->priv.cap * 2;
    if (svWith.size + 1 > newCap) newCap = svWith.size + 1;
    char* pNew = k_IAllocatorMalloc(pAlloc, newCap);
    if (!pNew) return false;

    memcpy(pNew, svWith.pData, svWith.size);
    pNew[svWith.size] = '\0';
    k_IAllocatorFree(pAlloc, s->priv.ptr.pData);
    s->priv.ptr.pData = pNew;
    s->priv.ptr.size = svWith.size;
    s->priv.cap = newCap;

    return true;
}

bool
k_StringPush(k_String* s, k_IAllocator* pAlloc, const char* pData, ssize_t size)
{
    if (s->priv.cap <= 0) s->priv.cap = K_STRING_SMALL_SIZE;

    k_StringView svThis = k_StringToSv(s);

    if (svThis.size + size >= s->priv.cap)
    {
        ssize_t newCap = s->priv.cap * 2;
        if (svThis.size + size + 1 > newCap) newCap = svThis.size + size + 1;
        char* pNew;

        if (s->priv.cap > K_STRING_SMALL_SIZE)
        {
            pNew = k_IAllocatorRealloc(pAlloc, svThis.pData, svThis.size, newCap);
            if (!pNew) return false;
        }
        else
        {
            pNew = k_IAllocatorMalloc(pAlloc, newCap);
            if (!pNew) return false;
            memcpy(pNew, svThis.pData, svThis.size);
        }

        svThis.pData = pNew;
        s->priv.ptr.pData = pNew;
        s->priv.cap = newCap;
    }

    memcpy(svThis.pData + svThis.size, pData, size);
    svThis.pData[svThis.size + size] = '\0';
    if (s->priv.cap > K_STRING_SMALL_SIZE) s->priv.ptr.size = svThis.size + size;

    return true;
}

bool
k_StringPushFront(k_String* s, k_IAllocator* pAlloc, const char* pData, ssize_t size)
{
    if (s->priv.cap <= 0) s->priv.cap = K_STRING_SMALL_SIZE;

    k_StringView svThis = k_StringToSv(s);

    if (svThis.size + size >= s->priv.cap)
    {
        ssize_t newCap = s->priv.cap * 2;
        if (svThis.size + size + 1 > newCap) newCap = svThis.size + size + 1;
        char* pNew;

        if (s->priv.cap > K_STRING_SMALL_SIZE)
        {
            pNew = k_IAllocatorRealloc(pAlloc, svThis.pData, svThis.size, newCap);
            if (!pNew) return false;
        }
        else
        {
            pNew = k_IAllocatorMalloc(pAlloc, newCap);
            if (!pNew) return false;
            memcpy(pNew, svThis.pData, svThis.size);
        }

        svThis.pData = pNew;
        s->priv.ptr.pData = pNew;
        s->priv.cap = newCap;
    }

    memmove(svThis.pData + size, svThis.pData, svThis.size);
    memcpy(svThis.pData, pData, size);

    svThis.pData[svThis.size + size] = '\0';
    if (s->priv.cap > K_STRING_SMALL_SIZE) s->priv.ptr.size = svThis.size + size;

    return true;
}

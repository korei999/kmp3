#include "file.h"

#include <stdio.h>

#if defined _WIN32
    #include <io.h>
    #include <direct.h>
#elif defined __unix__
    #include <unistd.h>
#endif

bool
k_file_isatty(int fd)
{
#if defined _WIN32
    return _isatty(fd);
#elif defined __unix__
    return isatty(fd);
#endif
}

k_Span
k_file_load(k_IAllocator* pAlloc, const char* ntsPath)
{
    k_Span sp = {0};

    FILE* pFile = fopen(ntsPath, "rb");
    if (!pFile) return sp;
    fseek(pFile, 0, SEEK_END);
    const ssize_t fileSize = ftell(pFile);
    void* pMem = k_IAllocatorMalloc(pAlloc, fileSize + 1);
    if (!pMem) return sp;
    rewind(pFile);
    const ssize_t nRead = fread(pMem, fileSize, 1, pFile);
    if (nRead != 1)
    {
        k_IAllocatorFree(pAlloc, pMem);
        return sp;
    }

    ((char*)pMem)[fileSize] = '\0';

    sp.pData = pMem;
    sp.size = fileSize + 1;

    return sp;
}

ssize_t
k_file_write(int fd, void* pBuff, ssize_t buffSize)
{
#if defined _WIN32
    return _write(fd, pBuff, buffSize);
#elif defined __unix__
    return write(fd, pBuff, buffSize);
#endif
}

k_String
k_file_cwd(k_IAllocator* pAlloc)
{
    char aBuff[500] = {0};

#if defined _WIN32
    _getcwd(aBuff, sizeof(aBuff));
#elif defined __unix__
    getcwd(aBuff, sizeof(aBuff));
#endif

    return k_StringCreateNts(pAlloc, aBuff);
}

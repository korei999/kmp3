#pragma once

#include "String.hh"
#include "logs.hh"
#include "defer.hh"

#if __has_include(<sys/stat.h>)

    #include <sys/stat.h>

#ifdef _WIN32
    #define ADT_USE_WIN32_STAT
#endif

#endif

namespace adt
{
namespace file
{

enum class TYPE : u8 { UNHANDLED, FILE, DIRECTORY };

[[nodiscard]] inline String
load(IAllocator* pAlloc, StringView svPath)
{
    FILE* pf = fopen(svPath.data(), "rb");
    if (!pf)
    {
        LOG_WARN("failed to open '{}' file\n", svPath);
        return {};
    }
    defer(fclose(pf));

    String ret {};

    fseek(pf, 0, SEEK_END);
    ssize size = ftell(pf) + 1;
    rewind(pf);

    ret.m_pData = reinterpret_cast<char*>(pAlloc->malloc(size, sizeof(char)));
    ret.m_size = size - 1;
    fread(ret.data(), 1, ret.size(), pf);
    ret.m_pData[ret.m_size] = '\0';

    return ret;
}

[[nodiscard]] inline StringView
getPathEnding(StringView svPath)
{
    ssize lastSlash = svPath.lastOf('/');

    if (lastSlash == NPOS || (lastSlash + 1) == svPath.size()) /* nothing after slash */
        return svPath;

    return StringView(&svPath[lastSlash + 1], &svPath[svPath.m_size - 1] - &svPath[lastSlash]);
}

[[nodiscard]] inline String
replacePathEnding(IAllocator* pAlloc, StringView svPath, StringView svEnding)
{
    ADT_ASSERT(pAlloc != nullptr, " ");

    ssize lastSlash = svPath.lastOf('/');
    StringView sNoEnding = {&svPath[0], lastSlash + 1};
    String r = StringCat(pAlloc, sNoEnding, svEnding);
    return r;
}

inline void
replacePathEnding(Span<char>* spBuff, StringView svPath, StringView svEnding)
{
    ADT_ASSERT(spBuff != nullptr, " ");

    ssize lastSlash = svPath.lastOf('/');
    StringView svNoEnding = {&svPath[0], lastSlash + 1};
    ssize n = print::toSpan(*spBuff, "{}{}", svNoEnding, svEnding);
    spBuff->m_size = n;
}

[[nodiscard]] inline String
appendDirPath(IAllocator* pAlloc, StringView svDir, StringView svPath)
{
    ADT_ASSERT(pAlloc != nullptr, " ");

    String newString {};
    ssize buffSize = svDir.size() + svPath.size() + 2;
    char* pData = pAlloc->zallocV<char>(buffSize);
    newString.m_pData = pData;

    if (svDir.endsWith("/"))
        newString.m_size = print::toBuffer(pData, buffSize - 1, "{}{}", svDir, svPath);
    else newString.m_size = print::toBuffer(pData, buffSize - 1, "{}/{}", svDir, svPath);

    return newString;
}

[[nodiscard]] inline TYPE
fileType(const char* ntsPath)
{
    struct stat st {};

    [[maybe_unused]] int err = ::stat(ntsPath, &st) != 0;
    if (err != 0)
    {
#ifndef NDEBUG
        fprintf(stderr, "stat(): err: %d\n", err);
#endif
        return TYPE::UNHANDLED;
    }

#ifdef ADT_USE_WIN32_STAT

    if ((st.st_mode & S_IFMT) == S_IFDIR)
        return TYPE::DIRECTORY;
    else if ((st.st_mode & S_IFREG))
        return TYPE::FILE;
    else return TYPE::UNHANDLED;

#else

    if (S_ISREG(st.st_mode))
        return TYPE::FILE;
    else if (S_ISDIR(st.st_mode))
        return TYPE::DIRECTORY;
    else return TYPE::UNHANDLED;

#endif
}

} /* namespace file */
} /* namespace adt */

#pragma once

#include "String.hh"
#include "logs.hh"
#include "defer.hh"

#if __has_include(<unistd.h>)

    #define ADT_USE_LINUX_FILE

    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>

#elif defined _WIN32

    #define ADT_USE_WIN32_FILE
    #define ADT_USE_WIN32_STAT

    #include <sys/stat.h>

#endif

namespace adt
{
namespace file
{

enum class TYPE : u8 { UNHANDLED, FILE, DIRECTORY };

[[nodiscard]] inline String
load(IAllocator* pAlloc, const char* ntsPath)
{
    FILE* pf = fopen(ntsPath, "rb");
    if (!pf)
    {
        LOG_WARN("failed to open '{}' file\n", ntsPath);
        return {};
    }
    ADT_DEFER( fclose(pf) );

    String ret {};

    fseek(pf, 0, SEEK_END);
    isize size = ftell(pf) + 1;
    rewind(pf);

    ret.m_pData = reinterpret_cast<char*>(pAlloc->malloc(size, sizeof(char)));
    ret.m_size = size - 1;
    fread(ret.data(), 1, ret.size(), pf);
    ret.m_pData[ret.m_size] = '\0';

    return ret;
}

template<isize SIZE>
[[nodiscard]] inline StringFixed<SIZE>
load(const char* ntsPath)
{
    FILE* pf = fopen(ntsPath, "rb");
    if (!pf)
    {
        LOG_WARN("failed to open '{}' file\n", ntsPath);
        return {};
    }
    ADT_DEFER( fclose(pf) );

    StringFixed<SIZE> ret {};

    fseek(pf, 0, SEEK_END);
    const isize size = utils::min(isize(ftell(pf)), SIZE - 1);
    rewind(pf);

    fread(ret.data(), 1, size, pf);

    return ret;
}

[[nodiscard]] inline StringView
getPathEnding(const StringView svPath)
{
    isize lastSlash = svPath.lastOf('/');

    if (lastSlash == NPOS || (lastSlash + 1) == svPath.size()) /* nothing after slash */
        return svPath;

    return StringView(const_cast<char*>(&svPath[lastSlash + 1]), &svPath[svPath.m_size - 1] - &svPath[lastSlash]);
}

[[nodiscard]] inline String
replacePathEnding(IAllocator* pAlloc, const StringView svPath, const StringView svEnding)
{
    ADT_ASSERT(pAlloc != nullptr, " ");

    isize lastSlash = svPath.lastOf('/');
    const StringView sNoEnding = {const_cast<char*>(&svPath[0]), lastSlash + 1};
    String r = StringCat(pAlloc, sNoEnding, svEnding);
    return r;
}

inline void
replacePathEnding(Span<char>* spBuff, const StringView svPath, const StringView svEnding)
{
    ADT_ASSERT(spBuff != nullptr, " ");

    isize lastSlash = svPath.lastOf('/');
    const StringView svNoEnding = {const_cast<char*>(&svPath[0]), lastSlash + 1};
    isize n = print::toSpan(*spBuff, "{}{}", svNoEnding, svEnding);
    spBuff->m_size = n;
}

[[nodiscard]] inline String
appendDirPath(IAllocator* pAlloc, const StringView svDir, const StringView svPath)
{
    ADT_ASSERT(pAlloc != nullptr, " ");

    String newString {};
    isize buffSize = svDir.size() + svPath.size() + 2;
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
        print::err("stat(): err: {}\n", err);
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

struct Mapped : public StringView
{
    using StringView::StringView;

    /* */

    void
    unmap()
    {
#ifdef ADT_USE_LINUX_FILE
        munmap(data(), size());
        *this = {};
#else

        ADT_ASSERT(false, "not implemented");

#endif
    }
};

[[nodiscard]] inline Mapped
map(const char* ntsPath)
{
#ifdef ADT_USE_LINUX_FILE

    int fd = open(ntsPath, O_RDONLY);
    if (fd == -1)
    {
        LOG_BAD("failed to open() '{}'\n", ntsPath);
        return {};
    }

    ADT_DEFER( close(fd) );

    struct stat sb {};
    if (fstat(fd, &sb) == -1)
    {
        LOG_ERR("fstat() failed\n");
        return {};
    }

    isize fileSize = sb.st_size;

    void* pData = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (pData == MAP_FAILED)
    {
        LOG_ERR("mmap() failed\n");
        return {};
    }

    return {static_cast<char*>(pData), fileSize};

#else

    ADT_ASSERT(false, "not implemented");
    return {};

#endif
}

} /* namespace file */
} /* namespace adt */

/* Just a simple directory file iterator, nothing else is cross platform at the moment. */
/* TODO: abstract directory properties */

#pragma once

#include "print.hh"

#if __has_include(<dirent.h>)

    #define ADT_USE_DIRENT
    #include <dirent.h>

#elif __has_include(<windows.h>)

    #define ADT_USE_WIN32DIR

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>

#else

    #error "unsupported platform"

#endif

namespace adt
{

#ifdef ADT_USE_DIRENT

struct Directory
{
    DIR* m_pDir {};
    dirent* m_pEntry {};

    /* */

    Directory() = default;
    Directory(const char* ntsPath);

    /* */

    explicit operator bool() const { return m_pDir != nullptr; }

    /* */

    bool close();

    /* */

    struct It
    {
        Directory* p {};
        int i {}; /* NPOS flag */

        /* */

        It(const Directory* pSelf, int _i)
            : p(const_cast<Directory*>(pSelf)), i(_i)
        {
            if (i == NPOS) return;

            while ((p->m_pEntry = readdir(p->m_pDir)))
            {
                if (strcmp(p->m_pEntry->d_name, ".") == 0 || strcmp(p->m_pEntry->d_name, "..") == 0)
                    continue;
                else break;
            }
        }

        It(int _i) : i(_i) {}

        /* */

        [[nodiscard]] StringView
        operator*()
        {
            if (!p || (p && !p->m_pEntry)) return {};

            usize n = strnlen(p->m_pEntry->d_name, sizeof(p->m_pEntry->d_name));
            return {p->m_pEntry->d_name, static_cast<isize>(n)};
        }

        It
        operator++()
        {
            p->m_pEntry = readdir(p->m_pDir);
            if (!p->m_pEntry) i = NPOS;

            return *this;
        }

        friend bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend bool operator!=(const It& l, const It& r) { return l.i != r.i; }
    };

    It begin() { return {this, 0}; }
    It end() { return {NPOS}; }

    It begin() const { return {this, 0}; }
    It end() const { return {NPOS}; }
};

inline
Directory::Directory(const char* ntsPath)
{
    m_pDir = opendir(ntsPath);
    if (!m_pDir)
    {
#ifndef NDEBUG
        print::err("failed to open '{}' directory\n", ntsPath);
#endif
        return;
    }
}

inline bool
Directory::close()
{
     int err = closedir(m_pDir);

#ifndef NDEBUG
     if (err != 0) print::err("closedir(): error '{}'\n", err);
#endif

     return err == 0;
}

#endif /* ADT_USE_DIRENT */

#ifdef ADT_USE_WIN32DIR

struct Directory
{
    char m_aBuff[sizeof(WIN32_FIND_DATA::cFileName) + 3] {};
    WIN32_FIND_DATA m_fileData {};
    HANDLE m_hFind {};

    /* */

    Directory() = default;
    Directory(const char* ntsPath);

    /* */

    explicit operator bool() const;

    /* */

    bool close();

    /* */

    struct It
    {
        Directory* p {};
        bool bStatus {};

        /* */

        It(const Directory* self, bool _bStatus)
            : p(const_cast<Directory*>(self)), bStatus(_bStatus)
        {
            if (!bStatus) return;

            do
            {
                if (strcmp(p->m_fileData.cFileName, ".") == 0 || 
                    strcmp(p->m_fileData.cFileName, "..") == 0)
                    continue;
                else break;
            }
            while (FindNextFile(p->m_hFind, &p->m_fileData) != 0);
        }

        It(bool _bStatus) : bStatus(_bStatus) {}

        /* */

        [[nodiscard]] StringView
        operator*()
        {
            if (!p) return {};

            return StringView(p->m_fileData.cFileName,
                strnlen(p->m_fileData.cFileName, sizeof(p->m_fileData.cFileName))
            );
        }

        It
        operator++()
        {
            if (FindNextFile(p->m_hFind, &p->m_fileData) == 0)
                bStatus = false;

            return *this;
        }

        friend bool operator==(const It& l, const It& r) { return l.bStatus == r.bStatus; }
        friend bool operator!=(const It& l, const It& r) { return l.bStatus != r.bStatus; }
    };

    It begin() { return {this, true}; }
    It end() { return {this, false}; }

    It begin() const { return {this, true}; }
    It end() const { return {this, false}; }
};

inline
Directory::Directory(const char* ntsPath)
{
    StringView sv = ntsPath;

    if (!sv.endsWith("/*"))
    {
        if (sv.size() < static_cast<isize>(sizeof(m_aBuff) - 4))
        {
            strncpy(m_aBuff, ntsPath, sizeof(m_aBuff));

            if (sv.endsWith("/"))
            {
                m_aBuff[sv.size()] = '*';
            }
            else
            {
                m_aBuff[sv.size()] = '/';
                m_aBuff[sv.size() + 1] = '*';
            }
        }
    }

    m_hFind = FindFirstFile(m_aBuff, &m_fileData);

    if (m_hFind == INVALID_HANDLE_VALUE)
    {
#ifndef NDEBUG
        print::err("failed to open '{}'\n", ntsPath);
#endif
        memset(m_aBuff, 0, sizeof(m_aBuff));
    }
}

inline
Directory::operator bool() const
{
    return m_hFind != nullptr && m_hFind != INVALID_HANDLE_VALUE;
}

inline bool
Directory::close()
{
    int err = FindClose(m_hFind);

#ifndef NDEBUG
    if (err == 0)
        print::err("FindClose(): failed '{}'\n", GetLastError());
#endif

    return err > 0;
}

#endif

} /* namespace adt */

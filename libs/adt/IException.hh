#pragma once

#include "print.inc"

#include <source_location>

#if __has_include(<unistd.h>)

    #include <unistd.h>

#elif defined _WIN32

    #include <direct.h>
    #define getcwd _getcwd

#endif

#define ADT_RUNTIME_EXCEPTION(CND)                                                                                     \
    if (!static_cast<bool>(CND))                                                                                       \
        throw adt::RuntimeException(#CND);

namespace adt
{

inline const char*
_currentWorkingDirectory()
{
    static char aBuff[300] {};
    return getcwd(aBuff, sizeof(aBuff) - 1);
}

/* implementing here to avoid a bunch of linking issues */
inline const char*
stripSourcePath(const char* ntsSourcePath)
{
    static const StringView svCwd = _currentWorkingDirectory();
    return ntsSourcePath + svCwd.size() + 1;
}

struct IException
{
    IException() = default;
    virtual ~IException() = default;

    /* */

    virtual void printErrorMsg(FILE* fp) const = 0;
    virtual StringView getMsg() const = 0;
};

struct RuntimeException : public IException
{
    StringFixed<128> m_sfMsg {};
    std::source_location m_loc {};


    /* */

    RuntimeException() = default;
    RuntimeException(const StringView svMsg, std::source_location loc = std::source_location::current())
        : m_sfMsg(svMsg), m_loc {loc} {}

    virtual ~RuntimeException() = default;

    /* */

    virtual void
    printErrorMsg(FILE* fp) const override
    {
        char aBuff[256] {};
        print::toSpan(aBuff, "RuntimeException: ({}, {}): {}\n", stripSourcePath(m_loc.file_name()), m_loc.line(), m_sfMsg);
        fputs(aBuff, fp);
    };

    virtual StringView
    getMsg() const override
    {
        return m_sfMsg;
    }
};

} /* namespace adt */

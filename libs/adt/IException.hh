#pragma once

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace adt
{

struct IException
{
    IException() = default;
    virtual ~IException() = default;

    /* */

    virtual void printErrorMsg(FILE* fp) const = 0;
    virtual const char* getMsg() const = 0;
};

struct RuntimeException : public IException
{
    const char* m_ntsMsg {};

    /* */

    RuntimeException() = default;
    RuntimeException(const char* ntsMsg) : m_ntsMsg(ntsMsg) {}
    virtual ~RuntimeException() = default;

    /* */

    virtual void
    printErrorMsg(FILE* fp) const override
    {
        char aBuff[128] {};
        snprintf(aBuff, sizeof(aBuff) - 1, "RuntimeException: '%s'\n", m_ntsMsg);
        fputs(aBuff, fp);
    };

    virtual const char* getMsg() const override { return m_ntsMsg; }
};

} /* namespace adt */

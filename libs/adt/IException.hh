#pragma once

#include "StringDecl.hh"
#include "printDecl.hh"

namespace adt
{

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

    /* */

    RuntimeException() = default;
    RuntimeException(const StringView svMsg) : m_sfMsg(svMsg) {}
    virtual ~RuntimeException() = default;

    /* */

    virtual void
    printErrorMsg(FILE* fp) const override
    {
        char aBuff[128] {};
        print::toSpan(aBuff, "RuntimeException: '{}'\n", m_sfMsg);
        fputs(aBuff, fp);
    };

    virtual StringView getMsg() const override { return m_sfMsg; }
};

} /* namespace adt */

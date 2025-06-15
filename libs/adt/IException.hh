#pragma once

#include "String.inc"
#include "print.inc"

#define ADT_RUNTIME_EXCEPTION_THROW(CND)                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!static_cast<bool>(CND))                                                                                   \
        {                                                                                                              \
            char aBuff[128] {};                                                                                        \
            const adt::isize n = adt::print::toSpan(                                                                   \
                aBuff,                                                                                                 \
                "({}, {})"                                                                                             \
                ": '{}' failed",                                                                                       \
                ADT_LOGS_FILE, __LINE__, #CND                                                                          \
            );                                                                                                         \
            throw adt::RuntimeException(StringView(aBuff, n));                                                         \
        }                                                                                                              \
    } while (0)

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
        print::toSpan(aBuff, "RuntimeException: {}\n", m_sfMsg);
        fputs(aBuff, fp);
    };

    virtual StringView
    getMsg() const override
    {
        return m_sfMsg;
    }
};

} /* namespace adt */

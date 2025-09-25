#pragma once

#include "print-inl.hh"

#include <source_location>
#include <exception>

#define ADT_RUNTIME_EXCEPTION(CND)                                                                                     \
    if (!static_cast<bool>(CND))                                                                                       \
        throw adt::RuntimeException(#CND);

#define ADT_RUNTIME_EXCEPTION_FMT(CND, ...)                                                                            \
    if (!static_cast<bool>(CND))                                                                                       \
    {                                                                                                                  \
        adt::RuntimeException ex;                                                                                      \
        auto& aMsgBuff = ex.m_sfMsg.data();                                                                            \
        adt::isize n = adt::print::toBuffer(                                                                           \
            aMsgBuff, sizeof(aMsgBuff) - 1, "(RuntimeException, {}, {}): condition '" #CND "' failed",                 \
            print::shorterSourcePath(__FILE__), __LINE__                                                               \
        );                                                                                                             \
        n += adt::print::toBuffer(aMsgBuff + n, sizeof(aMsgBuff) - 1 - n, "\nMsg: ");                                  \
        n += adt::print::toBuffer(aMsgBuff + n, sizeof(aMsgBuff) - 1 - n, __VA_ARGS__);                                \
        throw ex;                                                                                                      \
    }

namespace adt
{

struct RuntimeException : public std::exception
{
    StringFixed<256> m_sfMsg {};


    /* */

    RuntimeException() = default;
    RuntimeException(const StringView svMsg, std::source_location loc = std::source_location::current()) noexcept;

    virtual ~RuntimeException() = default;

    /* */

    virtual const char* what() const noexcept override { return m_sfMsg.data(); }
};

inline
RuntimeException::RuntimeException(const StringView svMsg, std::source_location loc) noexcept
{
    print::toSpan(m_sfMsg.data(),
        "(RuntimeException, {}, {}): '{}'\n",
        print::shorterSourcePath(loc.file_name()),
        loc.line(),
        svMsg
    );
}

} /* namespace adt */

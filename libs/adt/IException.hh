#pragma once

namespace adt
{

struct IException
{
    IException() = default;

    /* */

    virtual ~IException() = default;

    /* */

    virtual void logErrorMsg() = 0;
    virtual const char* getMsg() = 0;
};

} /* namespace adt */

#pragma once

#include "String.hh"
#include "logs.hh"
#include "Opt.hh"
#include "defer.hh"

namespace adt
{
namespace file
{

[[nodiscard]] inline Opt<String>
load(IAllocator* pAlloc, String sPath)
{
    FILE* pf = fopen(sPath.data(), "rb");
    if (!pf)
    {
        LOG_WARN("Error opening '{}' file\n", sPath);
        return {};
    }
    defer(fclose(pf));

    String ret {};

    fseek(pf, 0, SEEK_END);
    ssize size = ftell(pf) + 1;
    rewind(pf);

    ret.m_pData = (char*)pAlloc->malloc(size, sizeof(char));
    ret.m_size = size - 1;
    fread(ret.data(), 1, ret.getSize(), pf);
    ret.m_pData[ret.m_size] = '\0';

    return ret;
}

[[nodiscard]]
inline String
getPathEnding(String sPath)
{
    ssize lastSlash = sPath.lastOf('/');

    if (lastSlash == NPOS || (lastSlash + 1) == sPath.getSize()) /* nothing after slash */
        return sPath;

    return String(&sPath[lastSlash + 1], &sPath[sPath.m_size - 1] - &sPath[lastSlash]);
}

[[nodiscard]]
inline String
replacePathEnding(IAllocator* pAlloc, String sPath, String sEnding)
{
    ADT_ASSERT(pAlloc != nullptr, " ");

    ssize lastSlash = sPath.lastOf('/');
    String sNoEnding = {&sPath[0], lastSlash + 1};
    String r = StringCat(pAlloc, sNoEnding, sEnding);
    return r;
}

inline void
replacePathEnding(Span<char>* pSpBuff, String sPath, String sEnding)
{
    ADT_ASSERT(pSpBuff != nullptr, " ");

    ssize lastSlash = sPath.lastOf('/');
    String sNoEnding = {&sPath[0], lastSlash + 1};
    ssize n = print::toSpan(*pSpBuff, "{}{}", sNoEnding, sEnding);
    pSpBuff->m_size = n;
}

} /* namespace file */
} /* namespace adt */

#pragma once

#include "String.hh"
#include "logs.hh"
#include "defer.hh"

namespace adt
{
namespace file
{

[[nodiscard]] inline String
load(IAllocator* pAlloc, StringView svPath)
{
    FILE* pf = fopen(svPath.data(), "rb");
    if (!pf)
    {
        LOG_WARN("Error opening '{}' file\n", svPath);
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

[[nodiscard]]
inline StringView
getPathEnding(StringView svPath)
{
    ssize lastSlash = svPath.lastOf('/');

    if (lastSlash == NPOS || (lastSlash + 1) == svPath.size()) /* nothing after slash */
        return svPath;

    return StringView(&svPath[lastSlash + 1], &svPath[svPath.m_size - 1] - &svPath[lastSlash]);
}

[[nodiscard]]
inline String
replacePathEnding(IAllocator* pAlloc, StringView svPath, StringView svEnding)
{
    ADT_ASSERT(pAlloc != nullptr, " ");

    ssize lastSlash = svPath.lastOf('/');
    StringView sNoEnding = {&svPath[0], lastSlash + 1};
    String r = StringCat(pAlloc, sNoEnding, svEnding);
    return r;
}

inline void
replacePathEnding(Span<char> spBuff, StringView svPath, StringView svEnding)
{
    ADT_ASSERT(spBuff != nullptr, " ");

    ssize lastSlash = svPath.lastOf('/');
    StringView sNoEnding = {&svPath[0], lastSlash + 1};
    ssize n = print::toSpan(spBuff, "{}{}", sNoEnding, svEnding);
    spBuff.m_size = n;
}

} /* namespace file */
} /* namespace adt */

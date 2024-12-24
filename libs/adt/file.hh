#pragma once

#include "String.hh"
#include "logs.hh"
#include "Opt.hh"
#include "defer.hh"

namespace adt
{
namespace file
{

struct Buff
{
    u8* m_pData {};
    u64 m_size {};

    /* */

    u8* data() { return m_pData; }
    const u8* data() const { return m_pData; }
    u64 getSize() { return m_size; }
};

template<typename BUFF_T = String>
[[nodiscard]]
inline Opt<BUFF_T>
load(IAllocator* pAlloc, String sPath)
{
    FILE* pf = fopen(sPath.data(), "rb");
    if (!pf)
    {
        LOG_WARN("Error opening '{}' file\n", sPath);
        return {};
    }
    defer(fclose(pf));

    BUFF_T ret {};

    fseek(pf, 0, SEEK_END);
    s64 size = ftell(pf) + 1;
    rewind(pf);

    ret.m_pData = (char*)pAlloc->malloc(size, sizeof(char));
    ret.m_size = size - 1;
    fread(ret.data(), 1, ret.getSize(), pf);

    return {ret, true};
}

[[nodiscard]]
constexpr String
getPathEnding(String sPath)
{
    u32 lastSlash = StringLastOf(sPath, '/');

    if (lastSlash == NPOS || (lastSlash + 1) == sPath.getSize()) /* nothing after slash */
        return {};

    return String(&sPath[lastSlash + 1], &sPath[sPath.m_size - 1] - &sPath[lastSlash]);
}

[[nodiscard]]
inline String
replacePathEnding(IAllocator* pAlloc, String sPath, String sEnding)
{
    u32 lastSlash = StringLastOf(sPath, '/');
    String sNoEnding = {&sPath[0], lastSlash + 1};
    String r = StringCat(pAlloc, sNoEnding, sEnding);
    return r;
}

} /* namespace file */
} /* namespace adt */

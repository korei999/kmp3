#pragma once

#include "types.hh"

namespace adt
{
namespace hash
{

template<typename T>
inline u64
func(T& x)
{
    return (x);
}

template<>
inline u64
func<u64>(u64& x)
{
    return x;
}

template<>
inline u64
func<void* const>(void* const& x)
{
    return u64(x) * 0xcbf29ce484222325;
}

constexpr u64
fnv(const char* str, u32 size)
{
    u64 hash = 0xcbf29ce484222325;
    for (u64 i = 0; i < size; i++)
        hash = (hash ^ u64(str[i])) * 0x100000001b3;
    return hash;
}

} /* namespace hash */
} /* namespace adt */

#pragma once

#include "String.hh" /* IWYU pragma: keep */

namespace adt::bin
{

inline u16
swapBytes(u16 x)
{
#ifdef _MSC_VER

    return _byteswap_ushort(x);

#elif __has_builtin(__builtin_bswap16)

    return  __builtin_bswap16(x);

#else

    return ((x & 0xff00u) >> 1 * 8) |
           ((x & 0x00ffu) << 1 * 8);

#endif
}

inline u32
swapBytes(u32 x)
{
#ifdef _MSC_VER

    return _byteswap_ulong(x);

#elif __has_builtin(__builtin_bswap32)

    return  __builtin_bswap32(x);

#else

    return ((x & 0xff000000u) >> 3 * 8) |
           ((x & 0x00ff0000u) >> 1 * 8) |
           ((x & 0x0000ff00u) << 1 * 8) |
           ((x & 0x000000ffu) << 3 * 8);

#endif
}

inline u64
swapBytes(u64 x)
{
#ifdef _MSC_VER

    return _byteswap_uint64(x);

#elif __has_builtin(__builtin_bswap64)

    return  __builtin_bswap64(x);

#else

    return ((x & 0xff00000000000000llu) >> 7 * 8) |
           ((x & 0x00ff000000000000llu) >> 5 * 8) |
           ((x & 0x0000ff0000000000llu) >> 2 * 8) |
           ((x & 0x000000ff00000000llu) >> 1 * 8) |
           ((x & 0x00000000ff000000llu) << 1 * 8) |
           ((x & 0x0000000000ff0000llu) << 3 * 8) |
           ((x & 0x000000000000ff00llu) << 5 * 8) |
           ((x & 0x00000000000000ffllu) << 7 * 8);

#endif
}

template<typename T>
inline constexpr T
maskBits(u64 nBits)
{
    return (nBits >= sizeof(T) * 8) ? ~T(0) : ((T(1) << nBits) - 1);
}

struct Reader
{
    StringView m_svFile {};
    isize m_pos {};

    /* */

    Reader() = default;
    Reader(StringView s) : m_svFile(s) {}

    /* */

    char& operator[](isize i) { ADT_ASSERT(i >= 0 && i < m_svFile.size(), "out of range: i: {}, size: {}\n", i, m_svFile.size()); return m_svFile[i]; }
    const char& operator[](isize i) const { ADT_ASSERT(i >= 0 && i < m_svFile.size(), "out of range: i: {}, size: {}\n", i, m_svFile.size()); return m_svFile[i]; }

    void skipBytes(isize n);
    StringView readString(isize bytes);
    u8 read8();
    u16 read16();
    u16 read16Rev();
    u32 read32();
    u32 read32Rev();
    u64 read64();
    u64 read64Rev();
    bool finished();
};

inline void 
Reader::skipBytes(isize n)
{
    m_pos += n;
}

inline StringView
Reader::readString(isize bytes)
{
    StringView ret(&m_svFile[m_pos], bytes);
    m_pos += bytes;
    return ret;
}

inline u8
Reader::read8()
{
    u32 ret = m_svFile.reinterpret<u8>(m_pos);
    m_pos += 1;
    return ret;
}

inline u16
Reader::read16()
{
    u32 ret = m_svFile.reinterpret<u16>(m_pos);
    m_pos += 2;
    return ret;
}

inline u16
Reader::read16Rev()
{
    return swapBytes(read16());
}

inline u32
Reader::read32()
{
    u32 ret = m_svFile.reinterpret<u32>(m_pos);
    m_pos += 4;
    return ret;
}

inline u32
Reader::read32Rev()
{
    return swapBytes(read32());
}

inline u64
Reader::read64()
{
    u64 ret = m_svFile.reinterpret<u64>(m_pos);
    m_pos += 8;
    return ret;
}

inline u64
Reader::read64Rev()
{
    return swapBytes(read64());
}

inline bool
Reader::finished()
{
    return m_pos >= m_svFile.size();
}

struct BitReader
{
    StringView m_svFile {};
    isize m_pos {};

    /* */

    BitReader() = default;
    BitReader(StringView svFile) : m_svFile(svFile) {}

    /* */

    template<typename T = u64>
    [[nodiscard]] T read(isize nBits);
};

template<typename T>
inline T
BitReader::read(isize nBits)
{
    T ret = 0;

    for (isize i = 0; i < nBits; ++i)
    {
        const isize byteI = (m_pos + i) / 8;
        const isize bitI = 7 - ((m_pos + i) % 8);

        if (byteI >= m_svFile.size())
            return ret;

        ret = (ret << 1) | ((m_svFile[byteI] >> bitI) & 1);
    }

    m_pos += nBits;
    return ret;
}

} /* namespace adt::bin */

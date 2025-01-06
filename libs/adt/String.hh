#pragma once

#include "IAllocator.hh"
#include "hash.hh"

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <immintrin.h>

namespace adt
{

[[nodiscard]] constexpr ssize
nullTermStringSize(const char* nts)
{
    ssize i = 0;
    if (!nts) return 0;

    while (nts[i] != '\0') ++i;

    return i;
}

struct String;

[[nodiscard]] inline bool operator==(const String& l, const String& r);
[[nodiscard]] inline bool operator==(const String& l, const char* r);
[[nodiscard]] inline bool operator!=(const String& l, const String& r);
[[nodiscard]] constexpr s64 operator-(const String& l, const String& r);

/* StringAlloc() inserts '\0' char */
[[nodiscard]] inline String StringAlloc(IAllocator* p, const char* str, ssize size);
[[nodiscard]] inline String StringAlloc(IAllocator* p, ssize size);
[[nodiscard]] inline String StringAlloc(IAllocator* p, const char* nts);
[[nodiscard]] inline String StringAlloc(IAllocator* p, const String s);

[[nodiscard]] inline String StringCat(IAllocator* p, const String l, const String r);

[[nodiscard]] inline ssize nGlyphs(const String str);

/* just pointer + size, no allocations */
struct String
{
    char* m_pData {};
    ssize m_size {};

    /* */

    constexpr String() = default;

    constexpr String(char* sNullTerminated)
        : m_pData(sNullTerminated), m_size(nullTermStringSize(sNullTerminated)) {}

    constexpr String(const char* sNullTerminated)
        : m_pData(const_cast<char*>(sNullTerminated)), m_size(nullTermStringSize(sNullTerminated)) {}

    constexpr String(char* pStr, ssize len)
        : m_pData(pStr), m_size(len) {}

    /* */

    constexpr char& operator[](ssize i)             { assert(i < m_size && "[String]: out of size"); return m_pData[i]; }
    constexpr const char& operator[](ssize i) const { assert(i < m_size && "[String]: out of size"); return m_pData[i]; }

    const char* data() const { return m_pData; }
    char* data() { return m_pData; }
    ssize getSize() const { return m_size; }
    [[nodiscard]] constexpr bool endsWith(const String r) const;
    [[nodiscard]] constexpr ssize lastOf(char c) const;
    void destroy(IAllocator* p);
    void trimEnd();
    void removeNLEnd(); /* remove \r\n */
    [[nodiscard]] bool contains(const String r) const;
    [[nodiscard]] String clone(IAllocator* pAlloc) const;

    /* */

    struct It
    {
        char* p;

        constexpr It(char* pFirst) : p{pFirst} {}

        constexpr char& operator*() { return *p; }
        constexpr char* operator->() { return p; }

        constexpr It operator++() { p++; return *this; }
        constexpr It operator++(int) { char* tmp = p++; return tmp; }
        constexpr It operator--() { p--; return *this; }
        constexpr It operator--(int) { char* tmp = p--; return tmp; }

        friend constexpr bool operator==(It l, It r) { return l.p == r.p; }
        friend constexpr bool operator!=(It l, It r) { return l.p != r.p; }
    };

    constexpr It begin() { return {&m_pData[0]}; }
    constexpr It end() { return {&m_pData[m_size]}; }
    constexpr It rbegin() { return {&m_pData[m_size - 1]}; }
    constexpr It rend() { return {m_pData - 1}; }

    constexpr const It begin() const { return {&m_pData[0]}; }
    constexpr const It end() const { return {&m_pData[m_size]}; }
    constexpr const It rbegin() const { return {&m_pData[m_size - 1]}; }
    constexpr const It rend() const { return {m_pData - 1}; }
};

/* wchar_t iterator for mutlibyte strings */
struct StringGlyphIt
{
    using Iter = String::It;

    /* */

    const String s;

    /* */

    StringGlyphIt(const String _s) : s(_s) {};

    /* */

    struct It
    {
        const char* p {};
        ssize i = 0;
        ssize size = 0;
        wchar_t wc {};

        constexpr It(const char* pFirst, ssize _i, ssize _size) : p{pFirst}, i(_i), size(_size) {}

        wchar_t& operator*() { return wc; }
        wchar_t* operator->() { return &wc; }

        It
        operator++()
        {
            if (i >= size)
            {
                i = NPOS;
                return *this;
            }

            int len = mbtowc(&wc, &p[i], size - i);
            i += len;
            return *this;
        }

        friend bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend bool operator!=(const It& l, const It& r) { return l.i != r.i; }
    };

    It begin() { return {s.data(), 0, s.getSize()}; }
    It end() { return {{}, NPOS, {}}; }

    const It begin() const { return {s.data(), 0, s.getSize()}; }
    const It end() const { return {{}, NPOS, {}}; }
};

constexpr bool
String::endsWith(const String r) const
{
    const auto& l = *this;

    if (l.m_size < r.m_size)
        return false;

    for (int i = r.m_size - 1, j = l.m_size - 1; i >= 0; --i, --j)
        if (r[i] != l[j])
            return false;

    return true;
}

constexpr bool
StringCmpSlow(const String l, const String r)
{
    if (l.m_size != r.m_size) return false;

    for (ssize i = 0; i < l.m_size; ++i)
        if (l[i] != r[i]) return false;

    return true;
}

ADT_NO_UB inline bool
StringCmpFast(const String& l, const String& r)
{
    if (l.m_size != r.m_size) return false;

    const usize* p0 = (usize*)l.m_pData;
    const usize* p1 = (usize*)r.m_pData;
    ssize len = l.m_size / 8;

    ssize i = 0;
    for (; i < len; ++i)
        if (p0[i] - p1[i] != 0) return false;

    if (l.m_size > 8)
    {
        const usize* t0 = (usize*)&l.m_pData[l.m_size - 9];
        const usize* t1 = (usize*)&r.m_pData[l.m_size - 9];
        return *t0 == *t1;
    }

    ssize leftOver = l.m_size - i*8;
    String nl(&l.m_pData[i*8], leftOver);
    String nr(&r.m_pData[i*8], leftOver);

    return StringCmpSlow(nl, nr);
}

#ifdef ADT_SSE4_2
inline bool
StringCmpSSE(const String& l, const String& r)
{
    if (l.getSize() != r.getSize()) return false;

    const __m128i* p0 = (__m128i*)l.data();
    const __m128i* p1 = (__m128i*)r.data();
    ssize len = l.getSize() / 16;

    ssize i = 0;
    for (; i < len; ++i)
    {
        auto res = _mm_xor_si128(_mm_loadu_si128(&p0[i]), _mm_loadu_si128(&p1[i]));
        if (_mm_testz_si128(res, res) != 1) return false;
    }

    if (l.getSize() > 16)
    {
        auto lv = _mm_loadu_si128((__m128i*)&l[l.getSize() - 17]);
        auto rv = _mm_loadu_si128((__m128i*)&r[l.getSize() - 17]);
        auto res = _mm_xor_si128(lv, rv);
        return _mm_testz_si128(res, res) == 1;
    }

    ssize leftOver = l.getSize() - i*16;
    String nl(const_cast<char*>(&l[i*16]), leftOver);
    String nr(const_cast<char*>(&r[i*16]), leftOver);

    return StringCmpFast(nl, nr);
}
#endif

#ifdef ADT_AVX2
inline bool
StringCmpAVX2(const String& l, const String& r)
{
    if (l.getSize() != r.getSize()) return false;

    const __m256i* p0 = (__m256i*)l.data();
    const __m256i* p1 = (__m256i*)r.data();
    ssize len = l.getSize() / 32;

    ssize i = 0;
    for (; i < len; ++i)
    {
        __m256i res = _mm256_xor_si256(_mm256_loadu_si256(&p0[i]), _mm256_loadu_si256(&p1[i]));
        if (_mm256_testz_si256(res, res) != 1) return false;
    }

    if (l.getSize() > 32)
    {
        auto lv = _mm256_loadu_si256((__m256i*)&l[l.getSize() - 33]);
        auto rv = _mm256_loadu_si256((__m256i*)&r[l.getSize() - 33]);
        __m256i res = _mm256_xor_si256(lv, rv);
        return _mm256_testz_si256(res, res) == 1;
    }

    ssize leftOver = l.getSize() - i*32;
    String nl(const_cast<char*>(&l[i*32]), leftOver);
    String nr(const_cast<char*>(&r[i*32]), leftOver);

    return StringCmpFast(nl, nr);
}
#endif

inline bool
operator==(const String& l, const String& r)
{
    if (l.m_size != r.m_size) return false;
    if (l.data() == r.data()) return true; /* if points to itself */
    return strncmp(l.m_pData, r.m_pData, l.m_size) == 0; /* strncmp is pretty fast actually */
}

inline bool
operator==(const String& l, const char* r)
{
    auto sr = String(r);
    return l == sr;
}

inline bool
operator!=(const String& l, const String& r)
{
    return !(l == r);
}

constexpr s64
operator-(const String& l, const String& r)
{
    if (l.m_size < r.m_size) return -1;
    else if (l.m_size > r.m_size) return 1;

    s64 sum = 0;
    for (ssize i = 0; i < l.m_size; i++)
        sum += (l[i] - r[i]);

    return sum;
}

constexpr ssize
String::lastOf(char c) const
{
    for (int i = m_size - 1; i >= 0; i--)
        if ((*this)[i] == c)
            return i;

    return NPOS;
}

inline String
StringAlloc(IAllocator* p, const char* str, ssize size)
{
    if (str == nullptr || size == 0) return {};

    char* pData = (char*)p->zalloc(size + 1, sizeof(char));
    strncpy(pData, str, size);
    pData[size] = '\0';

    String sNew {pData, size};
    return sNew;
}

inline String
StringAlloc(IAllocator* p, ssize size)
{
    if (size == 0) return {};

    char* pData = (char*)p->zalloc(size + 1, sizeof(char));

    String sNew {pData, size};
    return sNew;
}

inline String
StringAlloc(IAllocator* p, const char* nts)
{
    return StringAlloc(p, nts, nullTermStringSize(nts));
}

inline String
StringAlloc(IAllocator* p, const String s)
{
    if (s.getSize() == 0) return {};

    char* pData = (char*)p->zalloc(s.getSize() + 1, sizeof(char));
    strncpy(pData, s.data(), s.getSize());
    pData[s.getSize()] = '\0';

    String sNew {pData, s.getSize()};
    return sNew;
}

inline void
String::destroy(IAllocator* p)
{
    p->free(m_pData);
    *this = {};
}

inline String
StringCat(IAllocator* p, const String l, const String r)
{
    ssize len = l.m_size + r.m_size;
    char* ret = (char*)p->zalloc(len + 1, sizeof(char));

    ssize pos = 0;
    for (ssize i = 0; i < l.m_size; ++i, ++pos)
        ret[pos] = l[i];
    for (ssize i = 0; i < r.m_size; ++i, ++pos)
        ret[pos] = r[i];

    ret[len] = '\0';

    return {ret, len};
}

inline void
String::trimEnd()
{
    auto isWhiteSpace = [&](int i) -> bool {
        char c = m_pData[i];
        if (c == '\n' || c == ' ' || c == '\r' || c == '\t' || c == '\0')
            return true;

        return false;
    };

    for (int i = m_size - 1; i >= 0; --i)
        if (isWhiteSpace(i))
        {
            m_pData[i] = 0;
            --m_size;
        }
        else break;
}

inline void
String::removeNLEnd()
{
    auto oneOf = [&](char c) -> bool {
        constexpr String chars = "\r\n";
        for (auto ch : chars)
            if (c == ch) return true;
        return false;
    };

    usize pos = m_size - 1;
    while (m_size > 0 && oneOf((*this)[pos]))
        m_pData[--m_size] = '\0';
}

inline bool
String::contains(const String r) const
{
    if (m_size < r.m_size || m_size == 0 || r.m_size == 0) return false;

    for (ssize i = 0; i < m_size - r.m_size + 1; ++i)
    {
        const String sSub {const_cast<char*>(&(*this)[i]), r.m_size};
        if (sSub == r) return true;
    }

    return false;
}

[[nodiscard]] inline String
String::clone(IAllocator* pAlloc) const
{
    return StringAlloc(pAlloc, *this);
}

inline ssize
nGlyphs(const String str)
{
    ssize n = 0;
    for (ssize i = 0; i < str.m_size; )
    {
        i+= mblen(&str[i], str.m_size - i);
        ++n;
    }

    return n;
}

template<>
constexpr usize
hash::func(const String& str)
{
    return hash::fnv(str.data(), str.getSize(), hash::FNV1_64_INIT);
}

namespace utils
{

[[nodiscard]] constexpr bool
empty(const String* s)
{
    return s->m_size == 0;
}

} /* namespace utils */

} /* namespace adt */

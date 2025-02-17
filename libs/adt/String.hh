#pragma once

#include "IAllocator.hh"
#include "hash.hh"
#include "Span.hh"

#include <cstring>
#include <cstdlib>

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
[[nodiscard]] inline i64 operator-(const String& l, const String& r);

/* StringAlloc() inserts '\0' char */
[[nodiscard]] inline String StringAlloc(IAllocator* p, const char* str, ssize size);
[[nodiscard]] inline String StringAlloc(IAllocator* p, ssize size);
[[nodiscard]] inline String StringAlloc(IAllocator* p, const char* nts);
[[nodiscard]] inline String StringAlloc(IAllocator* p, const String s);

[[nodiscard]] inline String StringCat(IAllocator* p, const String l, const String r);

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

    constexpr String(Span<char> sp)
        : String(sp.data(), sp.getSize()) {}

    /* */

#define ADT_RANGE_CHECK ADT_ASSERT(i >= 0 && i < m_size, "i: %lld, m_size: %lld", i, m_size);

    char& operator[](ssize i)             { ADT_RANGE_CHECK; return m_pData[i]; }
    const char& operator[](ssize i) const { ADT_RANGE_CHECK; return m_pData[i]; }

#undef ADT_RANGE_CHECK

    const char* data() const { return m_pData; }
    char* data() { return m_pData; }
    ssize getSize() const { return m_size; }
    [[nodiscard]] bool beginsWith(const String r) const;
    [[nodiscard]] bool endsWith(const String r) const;
    [[nodiscard]] ssize lastOf(char c) const;
    void destroy(IAllocator* p);
    void trimEnd();
    void removeNLEnd(); /* remove \r\n */
    [[nodiscard]] bool contains(const String r) const;
    [[nodiscard]] String clone(IAllocator* pAlloc) const;
    [[nodiscard]] char& first();
    [[nodiscard]] const char& first() const;
    [[nodiscard]] char& last();
    [[nodiscard]] const char& last() const;
    [[nodiscard]] ssize nGlyphs() const;

    template<typename T>
    [[nodiscard]] T reinterpret(ssize at) const;

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
    const String m_s;

    /* */

    StringGlyphIt(const String s) : m_s(s) {};

    /* */

    struct It
    {
        const char* p {};
        ssize i = 0;
        ssize size = 0;
        wchar_t wc {};

        It(const char* pFirst, ssize _i, ssize _size)
            : p{pFirst}, i(_i), size(_size)
        {
            operator++();
        }

        wchar_t& operator*() { return wc; }
        wchar_t* operator->() { return &wc; }

        It
        operator++()
        {
            if (!p || i < 0 || i >= size)
            {
death:
                i = NPOS;
                return *this;
            }

            int len = 0;

            len = mbtowc(&wc, &p[i], size - i);

            if (len == -1)
                goto death;
            else if (len == 0)
                len = 1;

            i += len;

            return *this;
        }

        friend bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend bool operator!=(const It& l, const It& r) { return l.i != r.i; }
    };

    It begin() { return {m_s.data(), 0, m_s.getSize()}; }
    It end() { return {{}, NPOS, {}}; }

    const It begin() const { return {m_s.data(), 0, m_s.getSize()}; }
    const It end() const { return {{}, NPOS, {}}; }
};

/* Separated by delimiter String iterator adapter */
struct StringWordIt
{
    const String m_sv {};
    const String m_svDelimiters {};

    /* */

    StringWordIt(const String sv, const String svDelimiters = " ") : m_sv(sv), m_svDelimiters(svDelimiters) {}

    struct It
    {
        String m_svCurrWord {};
        const String m_svStr;
        const String m_svSeps {};
        ssize m_i = 0;

        /* */

        It(const String sv, ssize pos, const String svSeps)
            : m_svStr(sv), m_svSeps(svSeps),  m_i(pos)
        {
            if (pos != NPOS)
                operator++();
        }

        /* */

        auto& operator*() { return m_svCurrWord; }
        auto* operator->() { return &m_svCurrWord; }

        It&
        operator++()
        {
            if (m_i >= m_svStr.getSize())
            {
                m_i = NPOS;
                return *this;
            }

            ssize start = m_i;
            ssize end = m_i;

            auto oneOf = [&](char c) -> bool
            {
                for (auto sep : m_svSeps)
                    if (c == sep)
                        return true;

                return false;
            };

            while (end < m_svStr.getSize() && !oneOf(m_svStr[end]))
                end++;

            m_svCurrWord = {const_cast<char*>(&m_svStr[start]), end - start};
            m_i = end + 1;

            return *this;
        }

        friend bool operator==(const It& l, const It& r) { return l.m_i == r.m_i; }
        friend bool operator!=(const It& l, const It& r) { return l.m_i != r.m_i; }
    };

    It begin() { return {m_sv, 0, m_svDelimiters}; }
    It end() { return {m_sv, NPOS, m_svDelimiters}; }

    const It begin() const { return {m_sv, 0, m_svDelimiters}; }
    const It end() const { return {m_sv, NPOS, m_svDelimiters}; }
};

inline bool
String::beginsWith(const String r) const
{
    const auto& l = *this;

    if (l.getSize() < r.getSize())
        return false;

    for (ssize i = 0; i < r.getSize(); ++i)
        if (l[i] != r[i])
            return false;

    return true;
}

inline bool
String::endsWith(const String r) const
{
    const auto& l = *this;

    if (l.m_size < r.m_size)
        return false;

    for (ssize i = r.m_size - 1, j = l.m_size - 1; i >= 0; --i, --j)
        if (r[i] != l[j])
            return false;

    return true;
}

inline bool
operator==(const String& l, const String& r)
{
    if (l.data() == r.data())
        return true;

    if (l.getSize() != r.getSize())
        return false;

    return strncmp(l.data(), r.data(), l.getSize()) == 0; /* strncmp is as fast as AVX2 version (on my 8700k) */
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

inline i64
operator-(const String& l, const String& r)
{
    if (l.m_size < r.m_size) return -1;
    else if (l.m_size > r.m_size) return 1;

    i64 sum = 0;
    for (ssize i = 0; i < l.m_size; i++)
        sum += (l[i] - r[i]);

    return sum;
}

inline ssize
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
    if (str == nullptr || size <= 0) return {};

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
    auto oneOf = [&](const char c) -> bool {
        constexpr String chars = "\r\n";
        for (const char ch : chars)
            if (c == ch) return true;
        return false;
    };

    while (m_size > 0 && oneOf(last()))
        m_pData[--m_size] = '\0';
}

inline bool
String::contains(const String r) const
{
    if (m_size < r.m_size || m_size == 0 || r.m_size == 0) return false;

    for (ssize i = 0; i < m_size - r.m_size + 1; ++i)
    {
        const String sSub {const_cast<char*>(&(*this)[i]), r.m_size};
        if (sSub == r)
            return true;
    }

    return false;
}

inline String
String::clone(IAllocator* pAlloc) const
{
    return StringAlloc(pAlloc, *this);
}

inline char&
String::first()
{
    return operator[](0);
}

inline const char&
String::first() const
{
    return operator[](0);
}

inline char&
String::last()
{
    return operator[](m_size - 1);
}

inline const char&
String::last() const
{
    return operator[](m_size - 1);
}

inline ssize
String::nGlyphs() const
{
    ssize n = 0;
    for (ssize i = 0; i < m_size; )
    {
        i+= mblen(&operator[](i), getSize() - i);
        ++n;
    }

    return n;
}

template<typename T>
ADT_NO_UB inline T
String::reinterpret(ssize at) const
{
    return *(T*)(&operator[](at));
}

template<>
inline usize
hash::func(const String& str)
{
    return hash::func(str.m_pData, str.getSize());
}

} /* namespace adt */

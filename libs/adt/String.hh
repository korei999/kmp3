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

#if defined __has_constexpr_builtin
    #if __has_constexpr_builtin(__builtin_strlen)

    i = __builtin_strlen(nts);

    #endif
#else

    while (nts[i] != '\0') ++i;

#endif

    return i;
}

struct StringView;
struct String;

[[nodiscard]] inline bool operator==(const StringView& l, const StringView& r);
[[nodiscard]] inline bool operator==(const StringView& l, const char* r);
[[nodiscard]] inline bool operator!=(const StringView& l, const StringView& r);
[[nodiscard]] inline i64 operator-(const StringView& l, const StringView& r);

inline String StringCat(IAllocator* p, const StringView& l, const StringView& r);

/* Just pointer + size, no allocations, has to be cloned into String to store safely */
struct StringView
{
    char* m_pData {};
    ssize m_size {};

    /* */

    constexpr StringView() = default;

    constexpr StringView(const char* sNullTerminated)
        : m_pData(const_cast<char*>(sNullTerminated)), m_size(nullTermStringSize(sNullTerminated)) {}

    constexpr StringView(char* pStr, ssize len)
        : m_pData(pStr), m_size(len) {}

    constexpr StringView(Span<char> sp)
        : StringView(sp.data(), sp.size()) {}

    /* */

    explicit constexpr operator bool() const { return size() > 0; }

    /* */

#define ADT_RANGE_CHECK ADT_ASSERT(i >= 0 && i < m_size, "i: %lld, m_size: %lld", i, m_size);

    constexpr char& operator[](ssize i)             { ADT_RANGE_CHECK; return m_pData[i]; }
    constexpr const char& operator[](ssize i) const { ADT_RANGE_CHECK; return m_pData[i]; }

#undef ADT_RANGE_CHECK

    constexpr const char* data() const { return m_pData; }
    constexpr char* data() { return m_pData; }
    constexpr ssize size() const { return m_size; }
    constexpr bool empty() const { return size() == 0; }
    [[nodiscard]] bool beginsWith(const StringView r) const;
    [[nodiscard]] bool endsWith(const StringView r) const;
    [[nodiscard]] ssize lastOf(char c) const;
    void trimEnd();
    void removeNLEnd(); /* remove \r\n */
    [[nodiscard]] bool contains(const StringView r) const;
    [[nodiscard]] char& first();
    [[nodiscard]] const char& first() const;
    [[nodiscard]] char& last();
    [[nodiscard]] const char& last() const;
    [[nodiscard]] ssize nGlyphs() const;

    template<typename T>
    T reinterpret(ssize at) const;

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
    const StringView m_s;

    /* */

    StringGlyphIt(const StringView s) : m_s(s) {};

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
GOTO_quit:
                i = NPOS;
                return *this;
            }

            int len = 0;

            len = mbtowc(&wc, &p[i], size - i);

            if (len == -1)
                goto GOTO_quit;
            else if (len == 0)
                len = 1;

            i += len;

            return *this;
        }

        friend bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend bool operator!=(const It& l, const It& r) { return l.i != r.i; }
    };

    It begin() { return {m_s.data(), 0, m_s.size()}; }
    It end() { return {{}, NPOS, {}}; }

    const It begin() const { return {m_s.data(), 0, m_s.size()}; }
    const It end() const { return {{}, NPOS, {}}; }
};

/* Separated by delimiter String iterator adapter */
struct StringWordIt
{
    const StringView m_sv {};
    const StringView m_svDelimiters {};

    /* */

    StringWordIt(const StringView sv, const StringView svDelimiters = " ") : m_sv(sv), m_svDelimiters(svDelimiters) {}

    struct It
    {
        StringView m_svCurrWord {};
        const StringView m_svStr;
        const StringView m_svSeps {};
        ssize m_i = 0;

        /* */

        It(const StringView sv, ssize pos, const StringView svSeps)
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
            if (m_i >= m_svStr.size())
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

            while (end < m_svStr.size() && !oneOf(m_svStr[end]))
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
StringView::beginsWith(const StringView r) const
{
    const auto& l = *this;

    if (l.size() < r.size())
        return false;

    for (ssize i = 0; i < r.size(); ++i)
        if (l[i] != r[i])
            return false;

    return true;
}

inline bool
StringView::endsWith(const StringView r) const
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
operator==(const StringView& l, const StringView& r)
{
    if (l.data() == r.data())
        return true;

    if (l.size() != r.size())
        return false;

    return strncmp(l.data(), r.data(), l.size()) == 0; /* strncmp is as fast as AVX2 version (on my 8700k) */
}

inline bool
operator==(const StringView& l, const char* r)
{
    auto sr = StringView(r);
    return l == sr;
}

inline bool
operator!=(const StringView& l, const StringView& r)
{
    return !(l == r);
}

inline i64
operator-(const StringView& l, const StringView& r)
{
    if (l.m_size < r.m_size) return -1;
    else if (l.m_size > r.m_size) return 1;

    i64 sum = 0;
    for (ssize i = 0; i < l.m_size; i++)
        sum += (l[i] - r[i]);

    return sum;
}

inline ssize
StringView::lastOf(char c) const
{
    for (int i = m_size - 1; i >= 0; i--)
        if ((*this)[i] == c)
            return i;

    return NPOS;
}

inline void
StringView::trimEnd()
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
StringView::removeNLEnd()
{
    auto oneOf = [&](const char c) -> bool {
        constexpr StringView chars = "\r\n";
        for (const char ch : chars)
            if (c == ch) return true;
        return false;
    };

    while (m_size > 0 && oneOf(last()))
        m_pData[--m_size] = '\0';
}

inline bool
StringView::contains(const StringView r) const
{
    if (m_size < r.m_size || m_size == 0 || r.m_size == 0) return false;

    for (ssize i = 0; i < m_size - r.m_size + 1; ++i)
    {
        const StringView sSub {const_cast<char*>(&(*this)[i]), r.m_size};
        if (sSub == r)
            return true;
    }

    return false;
}

inline char&
StringView::first()
{
    return operator[](0);
}

inline const char&
StringView::first() const
{
    return operator[](0);
}

inline char&
StringView::last()
{
    return operator[](m_size - 1);
}

inline const char&
StringView::last() const
{
    return operator[](m_size - 1);
}

inline ssize
StringView::nGlyphs() const
{
    ssize n = 0;
    for (ssize i = 0; i < m_size; )
    {
        i+= mblen(&operator[](i), size() - i);
        ++n;
    }

    return n;
}

template<typename T>
ADT_NO_UB inline T
StringView::reinterpret(ssize at) const
{
    return *(T*)(&operator[](at));
}

struct String : public StringView
{
    String() = default;
    String(IAllocator* pAlloc, const char* pChars, ssize size);
    String(IAllocator* pAlloc, const char* nts);
    String(IAllocator* pAlloc, Span<char> spChars);
    String(IAllocator* pAlloc, const StringView sv);

    /* */

    void destroy(IAllocator* pAlloc);
};

inline
String::String(IAllocator* pAlloc, const char* pChars, ssize size)
{
    if (pChars == nullptr || size <= 0)
        return;

    char* pNewData = pAlloc->mallocV<char>(size + 1);
    strncpy(pNewData, pChars, size);
    pNewData[size] = '\0';

    m_pData = pNewData;
    m_size = size;
}

inline
String::String(IAllocator* pAlloc, const char* nts)
    : String(pAlloc, nts, nullTermStringSize(nts)) {}

inline
String::String(IAllocator* pAlloc, Span<char> spChars)
    : String(pAlloc, spChars.data(), spChars.size()) {}

inline
String::String(IAllocator* pAlloc, const StringView sv)
    : String(pAlloc, sv.data(), sv.size()) {}

inline void
String::destroy(IAllocator* pAlloc)
{
    pAlloc->free(m_pData);
    *this = {};
}

inline String
StringCat(IAllocator* p, const StringView& l, const StringView& r)
{
    ssize len = l.size() + r.size();
    char* ret = (char*)p->zalloc(len + 1, sizeof(char));

    ssize pos = 0;
    for (ssize i = 0; i < l.size(); ++i, ++pos)
        ret[pos] = l[i];
    for (ssize i = 0; i < r.size(); ++i, ++pos)
        ret[pos] = r[i];

    ret[len] = '\0';

    String sNew;
    sNew.m_pData = ret;
    sNew.m_size = len;
    return sNew;
}

template<>
inline usize
hash::func(const StringView& str)
{
    return hash::func(str.m_pData, str.size());
}

} /* namespace adt */

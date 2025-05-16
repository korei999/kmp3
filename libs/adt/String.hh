#pragma once

#include "StringDecl.hh"

#include "assert.hh"
#include "IAllocator.hh"
#include "utils.hh"
#include "hash.hh"
#include "Span.hh" /* IWYU pragma: keep */
#include "print.hh" /* IWYU pragma: keep */

#include <cstdlib>
#include <cwchar>

namespace adt
{

[[nodiscard]] constexpr isize
ntsSize(const char* nts)
{
    isize i = 0;
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

template <isize SIZE>
[[nodiscard]] constexpr isize
charBuffStringSize(const char (&aCharBuff)[SIZE])
{
    if (SIZE == 0) return 0;

    isize i = 0;
    while (i < SIZE && aCharBuff[i] != '\0') ++i;

    return i;
}

inline constexpr
StringView::StringView(const char* nts)
    : m_pData(const_cast<char*>(nts)), m_size(ntsSize(nts)) {}

inline constexpr
StringView::StringView(char* pStr, isize len)
    : m_pData(pStr), m_size(len) {}

inline constexpr
StringView::StringView(Span<char> sp)
    : StringView(sp.data(), sp.size()) {}

template<isize SIZE>
inline constexpr
StringView::StringView(const char (&aCharBuff)[SIZE])
    : m_pData(const_cast<char*>(aCharBuff)),
      m_size(charBuffStringSize(aCharBuff)) {}

#define ADT_RANGE_CHECK ADT_ASSERT(i >= 0 && i < m_size, "i: {}, m_size: {}", i, m_size);

inline constexpr char&
StringView::operator[](isize i)
{
    ADT_RANGE_CHECK; return m_pData[i];
}

constexpr const char&
StringView::operator[](isize i) const
{
    ADT_RANGE_CHECK; return m_pData[i];
}

#undef ADT_RANGE_CHECK

/* wchar_t iterator for mutlibyte strings */
struct StringGlyphIt
{
    const StringView m_s {};

    /* */

    StringGlyphIt(const StringView s) : m_s {s} {};

    /* */

    struct It
    {
        const char* p {};
        isize i = 0;
        isize size = 0;
        wchar_t wc {};
        mbstate_t state {};

        It(const char* pFirst, isize _i, isize _size)
            : p {pFirst}, i {_i}, size {_size}
        {
            /* first char */
            operator++();
        }

        It(isize npos) : i {npos} {}

        wchar_t& operator*() { return wc; }
        wchar_t* operator->() { return &wc; }

        It
        operator++()
        {
            if (!p || i < 0 || i >= size)
            {
GOTO_quit:
                i = NPOS;
                return {NPOS};
            }

            int len = mbrtowc(&wc, &p[i], size - i, &state);

            if (len == -1) goto GOTO_quit;
            else if (len == 0 || len < -1) len = 1;

            i += len;

            return *this;
        }

        friend bool operator==(const It& l, const It& r) { return l.i == r.i; }
        friend bool operator!=(const It& l, const It& r) { return l.i != r.i; }
    };

    It begin() { return {m_s.data(), 0, m_s.size()}; }
    It end() { return {NPOS}; }

    const It begin() const { return {m_s.data(), 0, m_s.size()}; }
    const It end() const { return {NPOS}; }
};

/* Separated by delimiters String iterator adapter */
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
        isize m_i = 0;

        /* */

        It(const StringView sv, isize pos, const StringView svSeps, bool)
            : m_svStr(sv), m_svSeps(svSeps),  m_i(pos)
        {
            if (pos != NPOS) operator++();
        }

        It(const StringView sv, isize pos, const StringView svSeps)
            : m_svStr(sv), m_svSeps(svSeps),  m_i(pos) {}

        explicit It(isize i) : m_i(i) {}

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

            isize start = m_i;
            isize end = m_i;

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

            if (m_svCurrWord.empty()) operator++();

            return *this;
        }

        friend bool operator==(const It& l, const It& r) { return l.m_i == r.m_i; }
        friend bool operator!=(const It& l, const It& r) { return l.m_i != r.m_i; }
    };

    It begin() { return {m_sv, 0, m_svDelimiters, true}; }
    It end() { return It(NPOS); }

    const It begin() const { return {m_sv, 0, m_svDelimiters, true}; }
    const It end() const { return It(NPOS); }
};

constexpr inline isize
StringView::idx(const char* const p) const
{
    isize i = p - m_pData;
    ADT_ASSERT(i >= 0 && i < size(), "out of range: idx: {}: size: {}", i, size());

    return i;
}

inline bool
StringView::beginsWith(const StringView r) const
{
    const auto& l = *this;

    if (l.size() < r.size())
        return false;

    for (isize i = 0; i < r.size(); ++i)
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

    for (isize i = r.m_size - 1, j = l.m_size - 1; i >= 0; --i, --j)
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

    return strncmp(l.data(), r.data(), l.size()) == 0; /* strncmp is as fast as handmade AVX2 function. */
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
    for (isize i = 0; i < l.m_size; --i)
        sum += (l[i] - r[i]);

    return sum;
}

inline constexpr isize
StringView::lastOf(char c) const
{
    for (int i = size() - 1; i >= 0; --i)
        if ((*this)[i] == c)
            return i;

    return NPOS;
}

inline constexpr isize
StringView::firstOf(char c) const
{
    for (int i = 0; i < size(); ++i)
        if ((*this)[i] == c)
            return i;

    return NPOS;
}

inline void
StringView::trimEnd()
{
    auto isWhiteSpace = [&](int i) -> bool
    {
        char c = m_pData[i];
        if (c == '\n' || c == ' ' || c == '\r' || c == '\t' || c == '\0')
            return true;

        return false;
    };

    for (int i = m_size - 1; i >= 0; --i)
    {
        if (isWhiteSpace(i))
        {
            m_pData[i] = 0;
            --m_size;
        }
        else break;
    }
}

inline void
StringView::removeNLEnd()
{
    auto oneOf = [&](const char c) -> bool
    {
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

    for (isize i = 0; i < m_size - r.m_size + 1; ++i)
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

inline isize
StringView::nGlyphs() const
{
    mbstate_t state {};
    isize n = 0;

    for (isize i = 0; i < m_size; )
    {
        i+= mbrlen(&operator[](i), size() - i, &state);
        ++n;
    }

    return n;
}

template<typename T>
ADT_NO_UB inline T
StringView::reinterpret(isize at) const
{
    return *(T*)(&operator[](at));
}

inline
String::String(IAllocator* pAlloc, const char* pChars, isize size)
{
    if (pChars == nullptr || size <= 0) return;

    char* pNewData = pAlloc->mallocV<char>(size + 1);
    memcpy(pNewData, pChars, size);
    pNewData[size] = '\0';

    m_pData = pNewData;
    m_size = size;
}

inline
String::String(IAllocator* pAlloc, const char* nts)
    : String(pAlloc, nts, ntsSize(nts)) {}

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

inline void
String::replaceWith(IAllocator* pAlloc, StringView svWith)
{
    if (svWith.empty())
    {
        destroy(pAlloc);
        return;
    }

    if (size() < svWith.size() + 1)
        m_pData = pAlloc->reallocV<char>(data(), 0, svWith.size() + 1);

    strncpy(data(), svWith.data(), svWith.size());
    m_size = svWith.size();
    data()[size()] = '\0';
}

template<int SIZE>
inline
StringFixed<SIZE>::StringFixed(const StringView svName)
{
    /* memcpy doesn't like nullptrs */
    if (!svName.data() || svName.size() <= 0) return;

    const isize size = utils::min(svName.size(), isize(SIZE - 1));
    memcpy(m_aBuff, svName.data(), size);
    m_aBuff[size] = '\0';
}

template<int SIZE>
template<int SIZE_B>
inline
StringFixed<SIZE>::StringFixed(const StringFixed<SIZE_B> other)
{
    const isize size = utils::min(SIZE - 1, SIZE_B);
    memcpy(m_aBuff, other.m_aBuff, size);
    m_aBuff[size] = '\0';
}

template<int SIZE>
inline isize
StringFixed<SIZE>::size() const
{
    return strnlen(m_aBuff, SIZE);
}

template<int SIZE>
inline void
StringFixed<SIZE>::destroy()
{
    memset(data(), 0, sizeof(m_aBuff));
}

template<int SIZE>
inline bool
StringFixed<SIZE>::operator==(const StringFixed<SIZE>& other) const
{
    return StringView(*this) == StringView(other);
}

template<int SIZE>
inline bool
StringFixed<SIZE>::operator==(const StringView sv) const
{
    return StringView(m_aBuff) == sv;
}

template<int SIZE>
template<isize ARRAY_SIZE>
inline bool
StringFixed<SIZE>::operator==(const char (&aBuff)[ARRAY_SIZE]) const
{
    return StringView(*this) == aBuff;
}

template<int SIZE_L, int SIZE_R>
inline bool
operator==(const StringFixed<SIZE_L>& l, const StringFixed<SIZE_R>& r)
{
    return StringView(l) == StringView(r);
}

inline String
StringCat(IAllocator* p, const StringView& l, const StringView& r)
{
    isize len = l.size() + r.size();
    char* ret = p->zallocV<char>(len + 1);

    isize pos = 0;
    for (isize i = 0; i < l.size(); ++i, ++pos)
        ret[pos] = l[i];
    for (isize i = 0; i < r.size(); ++i, ++pos)
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

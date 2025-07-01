#pragma once

#include "String.inc"

#include "assert.hh"
#include "IAllocator.hh"
#include "utils.hh"
#include "Span.hh" /* IWYU pragma: keep */
#include "print.hh" /* IWYU pragma: keep */
#include "wcwidth.hh"
#include "hash.hh"

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

template<isize SIZE>
[[nodiscard]] constexpr isize
charBuffStringSize(const char (&aCharBuff)[SIZE])
{
    if (SIZE == 0) return 0;

    isize i = 0;
    while (i < SIZE && aCharBuff[i] != '\0') ++i;

    return i;
}

inline constexpr int
wcWidth(wchar_t wc)
{
    return mk_wcwidth(wc);
}

inline constexpr
StringView::StringView(const char* nts)
    : m_pData(const_cast<char*>(nts)), m_size(ntsSize(nts)) {}

inline constexpr
StringView::StringView(char* pStr, isize len)
    : m_pData(pStr), m_size(len) {}

inline constexpr
StringView::StringView(Span<char> sp)
    : StringView(sp.m_pData, sp.m_size) {}

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
struct StringWCharIt
{
    const StringView m_s {};

    /* */

    StringWCharIt(const StringView s) : m_s {s} {};

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

/* Grapheme cluster iterator */
struct StringGraphemeIt
{
    const StringView m_sv {};

    StringGraphemeIt(const StringView sv) : m_sv {sv} {};

    /* */

    static constexpr bool
    isRegional(const wchar_t wc)
    {
        return u32(wc) >= 0x1F1E6 && u32(wc) <= 0x1F1FF;
    }

    /* */

    struct It
    {
        const char* m_pStr {};
        isize m_size {};
        isize m_i {};
        isize m_clusterI {};
        isize m_clusterSize {};
        mbstate_t m_mbState {};

        /* */

        It(isize _NPOS) : m_i {_NPOS} {}
        It(const StringView sv) : m_pStr {sv.m_pData}, m_size {sv.m_size} { operator++(); }

        /* */

        const StringView operator*() const { return {const_cast<char*>(&m_pStr[m_clusterI]), m_clusterSize}; }

        It
        operator++()
        {
            if (!m_pStr || m_i >= m_size)
            {
                m_i = NPOS;
                return NPOS;
            }

            wchar_t wc1;
            int nBytesDecoded = mbrtowc(&wc1, &m_pStr[m_i], m_size - m_i, &m_mbState);
            if (nBytesDecoded == -1 || nBytesDecoded == -2)
            {
                /* just skip */
                nBytesDecoded = 1;
                wc1 = L' ';
                m_mbState = {};
            }

            m_clusterI = m_i;
            m_i += nBytesDecoded;

            if (isRegional(wc1))
            {
                mbstate_t mb2 {};
                wchar_t wc2;
                int nBytesDecoded2 = mbrtowc(&wc2, &m_pStr[m_i], m_size - m_i, &mb2);
                if (nBytesDecoded2 > 0 && isRegional(wc2))
                    m_i += nBytesDecoded2;
            }

            while (m_i < m_size)
            {
                mbstate_t mb2 {};
                wchar_t wc2;
                int nBytesDecoded2 = mbrtowc(&wc2, &m_pStr[m_i], m_size - m_i, &mb2);
                if (nBytesDecoded2 == 0 || nBytesDecoded2 == -1 || nBytesDecoded2 == -2)
                    break;

                /* zero-width joiner (ZWJ) */
                if (wc2 == 0x200D)
                {
                    m_i += nBytesDecoded2;
                    mbstate_t mb3 {};
                    wchar_t wc3;
                    int nBytesDecoded3 = mbrtowc(&wc3, &m_pStr[m_i], m_size - m_i, &mb3);
                    if (nBytesDecoded3 > 0)
                    {
                        m_i += nBytesDecoded3;
                        continue;
                    }
                    break;
                }

                if (wcWidth(wc2) != 0)
                    break;
                m_i += nBytesDecoded2;
            }

            m_clusterSize = m_i - m_clusterI;
            return *this;
        }

        /* */

        friend bool operator==(const It& l, const It& r) { return l.m_i == r.m_i; }
        friend bool operator!=(const It& l, const It& r) { return l.m_i != r.m_i; }
    };

    It begin() { return m_sv; }
    It end() { return NPOS; }
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
            if (m_i >= m_svStr.m_size)
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

            while (end < m_svStr.m_size && !oneOf(m_svStr[end]))
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

    if (l.m_size < r.m_size)
        return false;

    for (isize i = 0; i < r.m_size; ++i)
        if (l.m_pData[i] != r.m_pData[i])
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
        if (r.m_pData[i] != l.m_pData[j])
            return false;

    return true;
}

inline bool
operator==(const StringView& l, const StringView& r)
{
    if (l.m_pData == r.m_pData)
        return true;

    if (l.m_size != r.m_size)
        return false;

    return strncmp(l.m_pData, r.m_pData, l.m_size) == 0; /* (glibc) strncmp is as fast as handmade SIMD optimized function. */
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
        sum += (l.m_pData[i] - r.m_pData[i]);

    return sum;
}

inline bool
operator<(const StringView& l, const StringView& r)
{
    const isize len = utils::min(l.m_size, r.m_size);
    const isize res = strncmp(l.m_pData, r.m_pData, len);

    if (res == 0) return l.m_size < r.m_size;
    else return res < 0;
}

inline bool
operator<=(const StringView& l, const StringView& r)
{
    return (l.m_size == r.m_size) || (l < r);
}

inline bool
operator>(const StringView& l, const StringView& r)
{
    const isize len = utils::min(l.m_size, r.m_size);
    const isize res = ::strncmp(l.m_pData, r.m_pData, len);

    if (res == 0) return l.m_size > r.m_size;
    else return res > 0;
}

inline bool
operator>=(const StringView& l, const StringView& r)
{
    return (l.m_size == r.m_size) || (l > r);
}

inline constexpr isize
StringView::lastOf(char c) const
{
    for (int i = m_size - 1; i >= 0; --i)
        if (m_pData[i] == c)
            return i;

    return NPOS;
}

inline constexpr isize
StringView::firstOf(char c) const
{
    for (int i = 0; i < m_size; ++i)
        if (m_pData[i] == c)
            return i;

    return NPOS;
}

inline StringView&
StringView::trimEnd()
{
    return trimEnd([](char*) {});
}

inline StringView&
StringView::removeNLEnd()
{
    return removeNLEnd([](char*) {});
}

inline bool
StringView::contains(const StringView r) const
{
    if (m_size < r.m_size || m_size == 0 || r.m_size == 0)
        return false;

#if __has_include(<unistd.h>)

    return memmem(m_pData, m_size, r.m_pData, r.m_size) != nullptr;

#else

    for (isize i = 0; i < m_size - r.m_size + 1; ++i)
    {
        const StringView svSub {const_cast<char*>(&m_pData[i]), r.m_size};
        if (svSub == r)
            return true;
    }

    return false;

#endif
}

inline isize
StringView::subStringAt(const StringView r) const noexcept
{
    if (m_size < r.m_size || m_size == 0 || r.m_size == 0)
        return false;

#if __has_include(<unistd.h>)

    const void* ptr = memmem(m_pData, m_size, r.m_pData, r.m_size);

    if (ptr) return idx(static_cast<const char*>(ptr));

#else

    for (isize i = 0; i < m_size - r.m_size + 1; ++i)
    {
        const StringView svSub {const_cast<char*>(&m_pData[i]), r.m_size};
        if (svSub == r)
            return i;
    }

#endif

    return -1;
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
StringView::multiByteSize() const
{
    mbstate_t state {};
    isize n = 0;

    for (isize i = 0; i < m_size; )
    {
        i+= mbrlen(&operator[](i), m_size - i, &state);
        ++n;
    }

    return n;
}

inline i64
StringView::toI64(int base) const noexcept
{
    char* pEnd = m_pData + m_size;
    return strtoll(m_pData, &pEnd, base);
}

inline u64
StringView::toU64(int base) const noexcept
{
    char* pEnd = m_pData + m_size;
    return strtoull(m_pData, &pEnd, base);
}

inline f64
StringView::toF64() const noexcept
{
    char* pEnd = m_pData + m_size;
    return strtod(m_pData, &pEnd);
}

template<typename T>
ADT_NO_UB inline T
StringView::reinterpret(isize at) const
{
    return *(T*)(&operator[](at));
}

template<typename LAMBDA>
inline StringView&
StringView::trimEnd(LAMBDA clFill)
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
            clFill(&m_pData[i]);
            --m_size;
        }
        else break;
    }

    return *this;
}

template<typename LAMBDA>
inline StringView&
StringView::removeNLEnd(LAMBDA clFill)
{
    auto oneOf = [&](const char c) -> bool
    {
        constexpr StringView chars = "\r\n";
        for (const char ch : chars)
            if (c == ch) return true;
        return false;
    };

    while (m_size > 0 && oneOf(last()))
    {
        --m_size;
        clFill(&m_pData[m_size]);
    }

    return *this;
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
    : String(pAlloc, spChars.m_pData, spChars.m_size) {}

inline
String::String(IAllocator* pAlloc, const StringView sv)
    : String(pAlloc, sv.m_pData, sv.m_size) {}

inline String&
String::trimEnd(bool bPadWithZeros)
{
    if (bPadWithZeros) StringView::trimEnd([](char* p) { *p = '\0'; });
    else StringView::trimEnd();

    return *this;
}

inline String&
String::removeNLEnd(bool bPadWithZeros)
{
    if (bPadWithZeros) StringView::removeNLEnd([](char* p) { *p = '\0'; });
    else StringView::removeNLEnd();

    return *this;
}

inline void
String::destroy(IAllocator* pAlloc) noexcept
{
    pAlloc->free(m_pData);
    *this = {};
}

inline void
String::reallocWith(IAllocator* pAlloc, const StringView svWith)
{
    if (svWith.empty())
    {
        destroy(pAlloc);
        return;
    }

    if (size() < svWith.size() + 1)
        m_pData = pAlloc->reallocV<char>(m_pData, 0, svWith.m_size + 1);

    strncpy(m_pData, svWith.m_pData, svWith.m_size);
    m_size = svWith.m_size;
    m_pData[m_size] = '\0';
}

inline String
String::release() noexcept
{
    return utils::exchange(this, {});
}

template<int SIZE>
inline
StringFixed<SIZE>::StringFixed(const StringView svName)
{
    /* memcpy doesn't like nullptrs */
    if (!svName.m_pData || svName.m_size <= 0) return;

    const isize size = utils::min(svName.m_size, isize(SIZE - 1));
    memcpy(m_aBuff, svName.m_pData, size);
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
StringFixed<SIZE>::size() const noexcept
{
    return strnlen(m_aBuff, SIZE);
}

template<int SIZE>
inline void
StringFixed<SIZE>::destroy() noexcept
{
    memset(m_aBuff, 0, sizeof(m_aBuff));
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
    for (isize i = 0; i < l.m_size; ++i, ++pos)
        ret[pos] = l[i];
    for (isize i = 0; i < r.m_size; ++i, ++pos)
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
    return hash::func(str.m_pData, str.m_size);
}

} /* namespace adt */

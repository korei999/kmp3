#pragma once

#include "SpanDecl.hh"

#include <cstring>

namespace adt
{

struct IAllocator;
struct String;

[[nodiscard]] constexpr ssize ntsSize(const char* nts);

template <ssize SIZE>
[[nodiscard]] constexpr ssize charBuffStringSize(const char (&aCharBuff)[SIZE]);

/* Just pointer + size, no allocations, has to be cloned into String to store safely */
struct StringView
{
    char* m_pData {};
    ssize m_size {};

    /* */

    constexpr StringView() = default;

    constexpr StringView(const char* nts);

    constexpr StringView(char* pStr, ssize len);

    constexpr StringView(Span<char> sp);

    template<ssize SIZE>
    constexpr StringView(const char (&aCharBuff)[SIZE]);

    /* */

    explicit constexpr operator bool() const { return size() > 0; }

    /* */

    constexpr char& operator[](ssize i);
    constexpr const char& operator[](ssize i) const;

    constexpr const char* data() const { return m_pData; }
    constexpr char* data() { return m_pData; }
    constexpr ssize size() const { return m_size; }
    constexpr bool empty() const { return size() == 0; }
    constexpr ssize idx(const char* const pChar) const;
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

        constexpr It(const char* pFirst) : p{const_cast<char*>(pFirst)} {}

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

[[nodiscard]] inline bool operator==(const StringView& l, const StringView& r);
[[nodiscard]] inline bool operator==(const StringView& l, const char* r);
[[nodiscard]] inline bool operator!=(const StringView& l, const StringView& r);
[[nodiscard]] inline i64 operator-(const StringView& l, const StringView& r);

inline String StringCat(IAllocator* p, const StringView& l, const StringView& r);

struct String : public StringView
{
    String() = default;
    String(IAllocator* pAlloc, const char* pChars, ssize size);
    String(IAllocator* pAlloc, const char* nts);
    String(IAllocator* pAlloc, Span<char> spChars);
    String(IAllocator* pAlloc, const StringView sv);

    /* */

    void destroy(IAllocator* pAlloc);
    void replaceWith(IAllocator* pAlloc, StringView svWith);
};

template<int SIZE> requires(SIZE > 1)
struct StringFixed
{
    char m_aBuff[SIZE] {};

    /* */

    StringFixed() = default;

    StringFixed(const StringView svName);

    StringFixed(const char* nts) : StringFixed(StringView(nts)) {}

    template<int SIZE_B>
    StringFixed(const StringFixed<SIZE_B> other);

    /* */

    operator adt::StringView() { return StringView(m_aBuff); };
    operator const adt::StringView() const { return StringView(m_aBuff); };

    /* */

    bool operator==(const StringFixed& other) const;
    bool operator==(const adt::StringView sv) const;

    char* data() { return m_aBuff; }
    const char* data() const { return m_aBuff; }

    ssize size() const;
};

} /* namespace adt */

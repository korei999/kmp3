#pragma once

#include "SpanDecl.hh"

#include <cstring> /* IWYU pragma: keep */

namespace adt
{

struct IAllocator;
struct String;

[[nodiscard]] constexpr isize ntsSize(const char* nts);

template <isize SIZE>
[[nodiscard]] constexpr isize charBuffStringSize(const char (&aCharBuff)[SIZE]);

/* Just pointer + size, no allocations, has to be cloned into String to store safely */
struct StringView
{
    char* m_pData {};
    isize m_size {};

    /* */

    constexpr StringView() = default;

    constexpr StringView(const char* nts);

    constexpr StringView(char* pStr, isize len);

    constexpr StringView(Span<char> sp);

    template<isize SIZE>
    constexpr StringView(const char (&aCharBuff)[SIZE]);

    /* */

    explicit constexpr operator bool() const { return size() > 0; }

    /* */

    constexpr char& operator[](isize i);
    constexpr const char& operator[](isize i) const;

    constexpr const char* data() const { return m_pData; }
    constexpr char* data() { return m_pData; }
    constexpr isize size() const { return m_size; }
    constexpr bool empty() const { return size() == 0; }
    constexpr isize idx(const char* const pChar) const;
    [[nodiscard]] bool beginsWith(const StringView r) const;
    [[nodiscard]] bool endsWith(const StringView r) const;
    constexpr isize lastOf(char c) const;
    constexpr isize firstOf(char c) const;
    void trimEnd();
    void removeNLEnd(); /* remove \r\n */
    [[nodiscard]] bool contains(const StringView r) const;
    [[nodiscard]] char& first();
    [[nodiscard]] const char& first() const;
    [[nodiscard]] char& last();
    [[nodiscard]] const char& last() const;
    [[nodiscard]] isize nGlyphs() const;

    template<typename T>
    T reinterpret(isize at) const;

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
    String(IAllocator* pAlloc, const char* pChars, isize size);
    String(IAllocator* pAlloc, const char* nts);
    String(IAllocator* pAlloc, Span<char> spChars);
    String(IAllocator* pAlloc, const StringView sv);

    /* */

    void destroy(IAllocator* pAlloc);
    void replaceWith(IAllocator* pAlloc, StringView svWith);
    String release(); /* return this String resource and set to zero */
};

/* Holds SIZE - 1 characters, terminates with '\0'. */
template<int SIZE>
struct StringFixed
{
    static constexpr isize CAP = SIZE;
    static_assert(SIZE > 1);

    /* */

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

    explicit operator bool() const { return size() > 0; }

    /* */

    bool operator==(const StringFixed& other) const;
    bool operator==(const adt::StringView sv) const;
    template<isize ARRAY_SIZE> bool operator==(const char (&aBuff)[ARRAY_SIZE]) const;

    auto& data() { return m_aBuff; }
    const auto& data() const { return m_aBuff; }

    isize size() const;
    void destroy();
};

template<int SIZE_L, int SIZE_R> inline bool operator==(const StringFixed<SIZE_L>& l, const StringFixed<SIZE_R>& r);

} /* namespace adt */

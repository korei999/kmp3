#pragma once

#include "Span.inc"
#include "hash.hh"

namespace adt
{

struct StdAllocatorNV;
struct IAllocator;
struct String;

[[nodiscard]] constexpr isize ntsSize(const char* nts);

template<isize SIZE>
[[nodiscard]] constexpr isize charBuffStringSize(const char (&aCharBuff)[SIZE]);

inline constexpr int wcWidth(wchar_t wc);

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
    constexpr bool empty() const { return m_size == 0; }
    constexpr isize idx(const char* const pChar) const;
    [[nodiscard]] bool beginsWith(const StringView r) const;
    [[nodiscard]] bool endsWith(const StringView r) const;
    constexpr isize lastOf(char c) const;
    constexpr isize firstOf(char c) const;
    void trimEnd();
    void removeNLEnd(); /* remove \r\n */
    [[nodiscard]] bool contains(const StringView r) const;
    [[nodiscard]] isize subStringAt(const StringView r) const noexcept;
    [[nodiscard]] char& first();
    [[nodiscard]] const char& first() const;
    [[nodiscard]] char& last();
    [[nodiscard]] const char& last() const;
    [[nodiscard]] isize multiByteSize() const;

    template<typename T>
    T reinterpret(isize at) const;

    /* */

    constexpr char* begin() { return &m_pData[0]; }
    constexpr char* end() { return &m_pData[m_size]; }
    constexpr char* rbegin() { return &m_pData[m_size - 1]; }
    constexpr char* rend() { return m_pData - 1; }

    constexpr const char* begin() const { return &m_pData[0]; }
    constexpr const char* end() const { return &m_pData[m_size]; }
    constexpr const char* rbegin() const { return &m_pData[m_size - 1]; }
    constexpr const char* rend() const { return m_pData - 1; }
};

[[nodiscard]] inline bool operator==(const StringView& l, const StringView& r);
[[nodiscard]] inline bool operator==(const StringView& l, const char* r);
[[nodiscard]] inline bool operator!=(const StringView& l, const StringView& r);
[[nodiscard]] inline i64 operator-(const StringView& l, const StringView& r);

inline String StringCat(IAllocator* p, const StringView& l, const StringView& r);

template<>
inline usize hash::func(const StringView& str);

struct String : public StringView
{
    String() = default;
    String(IAllocator* pAlloc, const char* pChars, isize size);
    String(IAllocator* pAlloc, const char* nts);
    String(IAllocator* pAlloc, Span<char> spChars);
    String(IAllocator* pAlloc, const StringView sv);

    /* */

    void destroy(IAllocator* pAlloc) noexcept;
    void reallocWith(IAllocator* pAlloc, const StringView svWith);
    [[nodiscard]] String release() noexcept; /* return this String resource and set to zero */
};

template<typename ALLOC_T = StdAllocatorNV>
struct StringManaged : protected ALLOC_T, public String
{
    StringManaged() = default;
    StringManaged(const char* pChars, isize size) : String {&allocator(), pChars, size} {}
    StringManaged(const char* nts) : String {&allocator(), nts} {}
    StringManaged(Span<char> spChars) : String {&allocator(), spChars} {}
    StringManaged(const StringView sv) : String {&allocator(), sv} {}

    /* */

    ALLOC_T& allocator() { return *static_cast<ALLOC_T*>(this); }
    const ALLOC_T& allocator() const { return *static_cast<ALLOC_T*>(this); }

    void destroy() noexcept { String::destroy(&allocator()); }
    void reallocWith(const StringView svWith) { String::reallocWith(&allocator(), svWith); }
    [[nodiscard]] String release() noexcept { String r = *this; *this = {}; return r; }
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

    StringFixed(const char* p, const isize size) : StringFixed(StringView {const_cast<char*>(p), size}) {}

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

    isize cap() const noexcept { return CAP; }

    isize size() const noexcept;
    void destroy() noexcept;
};

template<int SIZE_L, int SIZE_R>
inline bool operator==(const StringFixed<SIZE_L>& l, const StringFixed<SIZE_R>& r);

} /* namespace adt */

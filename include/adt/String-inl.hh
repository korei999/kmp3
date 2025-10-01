#pragma once

#include "Span-inl.hh"

#include <concepts>

namespace adt
{

struct GpaNV;
struct IAllocator;
struct String;
struct StringView;

namespace utils
{
[[nodiscard]] inline isize compare(const StringView& l, const StringView& r);
[[nodiscard]] inline isize compareRev(const StringView& l, const StringView& r);
} /* namespace utils */

template<typename T>
concept ConvertsToStringView =
(requires(const T& t)
{
    { t.data() } -> std::same_as<const char*>;
    { t.size() } -> std::convertible_to<isize>;
}) ||
(requires(const T& t)
{
    { StringView(t) } -> std::same_as<StringView>;
});

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

    constexpr StringView(const Span<const char> sp) noexcept;

    constexpr StringView(const Span<const char> sp, isize size) noexcept;

    template<isize SIZE>
    constexpr StringView(const char (&aCharBuff)[SIZE]);

    /* */

    explicit constexpr operator bool() const noexcept { return size() > 0; }

    [[nodiscard]] inline bool operator==(const StringView& r) const noexcept;
    [[nodiscard]] inline bool operator==(const char* r) const noexcept;
    [[nodiscard]] inline bool operator!=(const StringView& r) const noexcept;
    [[nodiscard]] inline bool operator<(const StringView& r) const noexcept;
    [[nodiscard]] inline bool operator<=(const StringView& r) const noexcept;
    [[nodiscard]] inline bool operator>(const StringView& r) const noexcept;
    [[nodiscard]] inline bool operator>=(const StringView& r) const noexcept;
    [[nodiscard]] inline i64 operator-(const StringView& r) const noexcept;

    /* */

    constexpr char& operator[](isize i);
    constexpr const char& operator[](isize i) const;

    constexpr const char* data() const { return m_pData; }
    constexpr char* data() { return m_pData; }
    constexpr isize size() const { return m_size; }
    constexpr bool empty() const { return m_size <= 0; }
    constexpr isize idx(const char* const pChar) const;
    [[nodiscard]] bool beginsWith(const StringView r) const;
    [[nodiscard]] bool endsWith(const StringView r) const;
    constexpr isize lastOf(char c) const;
    constexpr isize firstOf(char c) const;
    StringView& trimEnd();
    StringView& removeNLEnd(); /* remove \r\n */
    [[nodiscard]] bool contains(const StringView r) const noexcept;
    [[nodiscard]] bool contains(char c) const noexcept;
    /* NPOS */ [[nodiscard]] isize subStringAt(const StringView r) const noexcept;
    /* NPOS */ [[nodiscard]] isize charAt(char c) const noexcept;
    [[nodiscard]] char& first();
    [[nodiscard]] const char& first() const;
    [[nodiscard]] char& last();
    [[nodiscard]] const char& last() const;
    [[nodiscard]] isize multiByteSize() const;
    [[nodiscard]] i64 toI64(int base = 10) const noexcept;
    [[nodiscard]] u64 toU64(int base = 10) const noexcept;
    [[nodiscard]] f64 toF64() const noexcept;
    [[nodiscard]] StringView subString(isize start, isize size) const noexcept;
    [[nodiscard]] StringView subString(isize start) const noexcept; /* From start + all the leftovers. */

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

protected:
    template<typename LAMBDA> StringView& trimEnd(LAMBDA clFill);
    template<typename LAMBDA> StringView& removeNLEnd(LAMBDA clFill);
};

[[nodiscard]] inline String StringCat(IAllocator* p, const StringView& l, const StringView& r);

struct String : public StringView
{
    String() = default;
    String(IAllocator* pAlloc, const char* pChars, isize size);
    String(IAllocator* pAlloc, const char* nts);
    String(IAllocator* pAlloc, const Span<const char> spChars);
    String(IAllocator* pAlloc, const Span<const char> spChars, isize size);
    String(IAllocator* pAlloc, const StringView sv);

    /* */

    String& trimEnd(bool bPadWithZeros);
    String& removeNLEnd(bool bPadWithZeros); /* remove \r\n */
    void destroy(IAllocator* pAlloc) noexcept;
    void reallocWith(IAllocator* pAlloc, const StringView svWith);
    [[nodiscard]] String release() noexcept; /* return this String resource and set to zero */
};

template<typename ALLOC_T = GpaNV>
struct StringManaged : public String
{
    StringManaged() = default;
    StringManaged(const char* pChars, isize size) : String {allocator(), pChars, size} {}
    StringManaged(const char* nts) : String {allocator(), nts} {}
    StringManaged(Span<char> spChars) : String {allocator(), spChars} {}
    StringManaged(const StringView sv) : String {allocator(), sv} {}
    StringManaged(const Span<const char> spChars, isize size) : String {allocator(), spChars.m_pData, size} {}

    /* */

    auto* allocator() const { return ALLOC_T::inst(); }

    void destroy() noexcept { String::destroy(allocator()); }
    void reallocWith(const StringView svWith) { String::reallocWith(allocator(), svWith); }
    [[nodiscard]] String release() noexcept { String r = *this; *this = {}; return r; }
};

using StringM = StringManaged<>;

/* Holds SIZE - 1 characters, terminates with '\0'. */
template<int SIZE>
struct StringFixed
{
    static constexpr isize CAP = SIZE;
    static_assert(SIZE > 1);

    /* */

    char m_aBuff[SIZE] {};

    /* */

    StringFixed() noexcept = default;
    StringFixed(const StringView svName) noexcept;
    StringFixed(const char* nts) noexcept : StringFixed(StringView(nts)) {}
    StringFixed(const char* p, const isize size) noexcept : StringFixed(StringView {const_cast<char*>(p), size}) {}
    StringFixed(const Span<const char> sp) noexcept : StringFixed(StringView {const_cast<char*>(sp.m_pData), sp.m_size}) {}
    StringFixed(const Span<const char> sp, isize size) noexcept : StringFixed(StringView {const_cast<char*>(sp.m_pData), size}) {}

    template<int SIZE_B> StringFixed(const StringFixed<SIZE_B> other) noexcept;

    /* */

    operator StringView() noexcept { return StringView(m_aBuff); };
    operator const StringView() const noexcept { return StringView(m_aBuff); };

    explicit operator bool() const noexcept { return size() > 0; }

    /* */

    bool operator==(const StringFixed& other) const noexcept;
    bool operator==(const StringView sv) const noexcept;
    template<isize ARRAY_SIZE> bool operator==(const char (&aBuff)[ARRAY_SIZE]) const noexcept;

    template<int SIZE_R> bool operator==(const StringFixed<SIZE_R>& r) const noexcept;

    auto& data() noexcept { return m_aBuff; }
    const auto& data() const noexcept { return m_aBuff; }

    isize cap() const noexcept { return CAP; }

    isize size() const noexcept;
    void destroy() noexcept;
};

/* Vec like, small string optimized String class. */
struct VString
{
    union {
        char m_aBuff[16] {};
        struct {
            char* pData;
            isize size;
        } m_allocated;
    };
    isize m_cap = 16;

    /* */

    VString() = default;
    VString(IAllocator* pAlloc, const StringView sv);
    VString(IAllocator* pAlloc, isize prealloc);

    operator StringView() noexcept { return StringView(data(), size()); }
    operator const StringView() const noexcept { return StringView(const_cast<char*>(data()), size()); }

    /* */

    void destroy(IAllocator* pAlloc) noexcept;
    bool steal(String* pStr) noexcept;

    char* data() noexcept;
    const char* data() const noexcept;

    bool empty() const noexcept;
    isize size() const noexcept;
    isize cap() const noexcept;

    isize push(IAllocator* pAlloc, char c);
    isize push(IAllocator* pAlloc, const StringView sv);

    void reallocWith(IAllocator* pAlloc, const StringView sv);
    void removeNLEnd(bool bDestructive) noexcept;

protected:
    void grow(IAllocator* pAlloc, isize newCap);
};

static_assert(sizeof(VString) == 24);

template<typename ALLOC_T = GpaNV>
struct VStringManaged : VString
{
    using Base = VString;

    VStringManaged() = default;
    VStringManaged(const StringView sv) : Base{ALLOC_T::inst(), sv} {}
    VStringManaged(isize prealloc) : Base{ALLOC_T::inst(), prealloc} {}

    /* */

    auto* allocator() const noexcept { return ALLOC_T::inst(); }
    void destroy() noexcept { Base::destroy(allocator()); }
    isize push(char c) { return Base::push(allocator(), c); }
    isize push(const StringView sv) { return Base::push(allocator(), sv); }
    void reallocWith(const StringView sv) { Base::reallocWith(allocator(), sv); }
};

using VStringM = VStringManaged<>;

} /* namespace adt */

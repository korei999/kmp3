/* Borrowed from: https://github.com/fmtlib/fmt
 * fmt/include/fmt/format.h */

/* Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * --- Optional exception to the license ---
 *
 * As an exception, if, as a result of your compiling your source code, portions
 * of this Software are embedded into a machine-executable object form of such
 * source code, you may redistribute such embedded portions in such object form
 * without including the above copyright and permission notices. */

#include "String-inl.hh"

#include <cstring>

namespace adt::utf8
{

inline constexpr const char*
codepoint(const char* s, u32* c, int* e)
{
    constexpr const int masks[] = {0x00, 0x7f, 0x1f, 0x0f, 0x07};
    constexpr const u32 mins[] = {4194304, 0, 128, 2048, 65536};
    constexpr const int shiftc[] = {0, 18, 12, 6, 0};
    constexpr const int shifte[] = {0, 6, 4, 2, 0};

    int len = "\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\0\0\0\0\0\0\0\2\2\2\2\3\3\4"[static_cast<unsigned char>(*s) >> 3];
    // Compute the pointer to the next character early so that the next
    // iteration can start working on the next character. Neither Clang
    // nor GCC figure out this reordering on their own.
    const char* next = s + len + !len;

    using uchar = unsigned char;

    // Assume a four-byte character and load four bytes. Unused bits are
    // shifted out.
    *c = u32(uchar(s[0]) & masks[len]) << 18;
    *c |= u32(uchar(s[1]) & 0x3f) << 12;
    *c |= u32(uchar(s[2]) & 0x3f) << 6;
    *c |= u32(uchar(s[3]) & 0x3f) << 0;
    *c >>= shiftc[len];

    // Accumulate the various error conditions.
    *e = (*c < mins[len]) << 6;      // non-canonical encoding
    *e |= ((*c >> 11) == 0x1b) << 7; // surrogate half?
    *e |= (*c > 0x10FFFF) << 8;      // out of range?
    *e |= (uchar(s[1]) & 0xc0) >> 2;
    *e |= (uchar(s[2]) & 0xc0) >> 4;
    *e |= uchar(s[3]) >> 6;
    *e ^= 0x2a; // top two bits of each tail byte correct?
    *e >>= shifte[len];

    return next;
}

constexpr inline u32 invalid_code_point = ~u32();

// Invokes f(cp, sv) for every code point cp in s with sv being the string view
// corresponding to the code point. cp is invalid_code_point on error.
template<typename F>
constexpr void
forEachCodepoint(const StringView s, F f)
{
    auto decode = [f](const char* buf_ptr, const char* ptr) {
        auto cp = u32();
        auto error = 0;
        auto end = codepoint(buf_ptr, &cp, &error);
        bool result =
            f(error ? invalid_code_point : cp,
              StringView(const_cast<char*>(ptr), error ? 1 : end - buf_ptr));
        return result ? (error ? buf_ptr + 1 : end) : nullptr;
    };

    auto p = s.data();
    constexpr isize block_size = 4; // utf8_decode always reads blocks of 4 chars.
    if (s.size() >= block_size)
    {
        for (auto end = p + s.size() - block_size + 1; p < end;)
        {
            p = decode(p, p);
            if (!p) return;
        }
    }
    auto num_chars_left = s.data() + s.size() - p;
    if (num_chars_left == 0)
        return;

    char buf[2 * block_size - 1] = {};
    ::memcpy(buf, p, num_chars_left);
    const char* buf_ptr = buf;
    do
    {
        auto end = decode(buf_ptr, p);
        if (!end) return;

        p += end - buf_ptr;
        buf_ptr = end;
    }
    while (buf_ptr < buf + num_chars_left);
}

inline constexpr isize
computeWidth(StringView s)
{
    isize num_code_points = 0;
    // It is not a lambda for compatibility with C++14.
    struct count_code_points
    {
        isize* count;
        constexpr auto operator()(u32 cp, StringView) const -> bool
        {
            *count += 
                1 + (cp >= 0x1100 && (cp <= 0x115f || // Hangul Jamo init. consonants
                                      cp == 0x2329 || // LEFT-POINTING ANGLE BRACKET
                                      cp == 0x232a || // RIGHT-POINTING ANGLE BRACKET
                                      // CJK ... Yi except IDEOGRAPHIC HALF FILL SPACE:
                                      (cp >= 0x2e80 && cp <= 0xa4cf && cp != 0x303f) ||
                                      (cp >= 0xac00 && cp <= 0xd7a3) ||   // Hangul Syllables
                                      (cp >= 0xf900 && cp <= 0xfaff) ||   // CJK Compatibility Ideographs
                                      (cp >= 0xfe10 && cp <= 0xfe19) ||   // Vertical Forms
                                      (cp >= 0xfe30 && cp <= 0xfe6f) ||   // CJK Compatibility Forms
                                      (cp >= 0xff00 && cp <= 0xff60) ||   // Fullwidth Forms
                                      (cp >= 0xffe0 && cp <= 0xffe6) ||   // Fullwidth Forms
                                      (cp >= 0x20000 && cp <= 0x2fffd) || // CJK
                                      (cp >= 0x30000 && cp <= 0x3fffd) ||
                                      // Miscellaneous Symbols and Pictographs + Emoticons:
                                      (cp >= 0x1f300 && cp <= 0x1f64f) ||
                                      // Supplemental Symbols and Pictographs:
                                      (cp >= 0x1f900 && cp <= 0x1f9ff)));
            return true;
        }
    };

    forEachCodepoint(s, count_code_points {&num_code_points});
    return num_code_points;
}

} /* adt::utf8 */

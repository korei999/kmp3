/*
* hash_64 - 64 bit Fowler/Noll/Vo-0 FNV-1a hash code
*
* @(#) $Revision: 5.1 $
* @(#) $Id: hash_64a.c,v 5.1 2009/06/30 09:01:38 chongo Exp $
* @(#) $Source: /usr/local/src/cmd/fnv/RCS/hash_64a.c,v $
*
***
*
* Fowler/Noll/Vo hash
*
* The basis of this hash algorithm was taken from an idea sent
* as reviewer comments to the IEEE POSIX P1003.2 committee by:
*
*      Phong Vo (http://www.research.att.com/info/kpv/)
*
* In a subsequent ballot round:
*
*      Landon Curt Noll (http://www.isthe.com/chongo/)
*
* improved on their algorithm.  Some people tried this hash
* and found that it worked rather well.  In an EMail message
* to Landon, they named it the ``Fowler/Noll/Vo'' or FNV hash.
*
* FNV hashes are designed to be fast while maintaining a low
* collision rate. The FNV speed allows one to quickly hash lots
* of data while maintaining a reasonable collision rate.  See:
*
*      http://www.isthe.com/chongo/tech/comp/fnv/index.html
*
* for more details as well as other forms of the FNV hash.
*
***
*
* To use the recommended 64 bit FNV-1a hash, pass FNV1A_64_INIT as the
* Fnv64_t hashval argument to fnv_64a_buf() or fnv_64a_str().
*
***
*
* Please do not copyright this code.  This code is in the public domain.
*
* LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
* INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
* EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
* CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
* USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
* OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
* PERFORMANCE OF THIS SOFTWARE.
*
* By:
*	chongo <Landon Curt Noll> /\oo/\ --
*      http://www.isthe.com/chongo/
*
* Share and Enjoy!	:-)
*/

#pragma once

#include "types.hh"

namespace adt
{
namespace hash
{

constexpr usize FNV1_64_INIT = 0xcbf29ce484222325ULL;

template<typename T>
inline constexpr usize
fnv(const T* pBuff, ssize byteSize, const usize seed)
{
    usize hval = seed;
    const char* p = (const char*)pBuff;

    for (ssize i = 0; i < byteSize; ++i)
    {
        hval ^= usize(p[i]);
        hval += (hval << 1) + (hval << 4) + (hval << 5) + (hval << 7) + (hval << 8) + (hval << 40);
    }

    return hval;
}

template<typename T>
inline usize
func(const T& x)
{
    return fnv((const char*)&x, sizeof(T), FNV1_64_INIT);
}

template<ssize N>
constexpr usize
func(const char (&aChars)[N])
{
    return fnv(aChars, N, FNV1_64_INIT);
}

template<typename T>
inline usize
func(const T* pBuff, ssize byteSize)
{
    return fnv((const char*)pBuff, byteSize, FNV1_64_INIT);
}

} /* namespace hash */
} /* namespace adt */

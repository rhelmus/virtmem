#ifndef UTILS_HPP
#define UTILS_HPP

#include "config.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

//#include <stdio.h> // UNDONE

namespace private_utils {

template <typename T, typename T2> struct TSameType { static const bool flag = false; };
template <typename T> struct TSameType<T, T> { static const bool flag = true; };

template <typename T> struct TVirtPtrTraits { };

template <typename T, typename A> struct TVirtPtrTraits<CVirtPtr<T, A> >
{
    typedef T TValue;
    typedef CPtrWrapLock<CVirtPtr<T, A> > TLock;

    static bool isWrapped(CVirtPtr<T, A> p) { return p.isWrapped(); }
    static T *unwrap(CVirtPtr<T, A> p) { return p.unwrap(); }
    static bool isVirtPtr(void) { return true; }
    static TLock makeLock(CVirtPtr<T, A> w, TVirtPageSize &s, bool f, bool ro=false)
    { return makeVirtPtrLock(w, s, f, ro); }
    static uint8_t getUnlockedBigPages(CVirtPtr<T, A>)
    { return A::getInstance()->getUnlockedBigPages(); }
    static TVirtPageSize getPageSize(void)
    { return A::getInstance()->getBigPageSize(); }

};

template <typename T> struct TVirtPtrTraits<T *>
{
    typedef T TValue;
    typedef T** TLock;

    static bool isWrapped(T *) { return false; }
    static T *unwrap(T *p) { return p; }
    static bool isVirtPtr(void) { return false; }
    static TLock makeLock(T *&p, TVirtPageSize &, bool, __attribute__ ((unused)) bool ro=false) { return &p; }
    static uint8_t getUnlockedBigPages(const T *) { return 0; }
    static TVirtPageSize getPageSize(void) { return (TVirtPageSize)-1; }
};

// dummy, not used
template <typename T1, typename T2> int ptrDiff(T1, T2) { return 0; }
template <typename T, typename A> int ptrDiff(CVirtPtr<T, A> p1, CVirtPtr<T, A> p2) { return p2 - p1; }

// copier function that works with raw pointers for rawCopy, returns false if copying should be aborted
typedef bool (*TRawCopier)(char *, const char *, TVirtPtrSize);

// Generalized copy for memcpy and strncpy
template <typename T1, typename T2> T1 rawCopy(T1 dest, T2 src, TVirtPtrSize size,
                                               TRawCopier copier, bool checknull)
{
    if (size == 0)
        return dest;

    if (!TVirtPtrTraits<T1>::isVirtPtr() && !TVirtPtrTraits<T2>::isVirtPtr())
    {
        // Even though both are pointers, we must unwrap to avoid compile errors with other types
        copier(TVirtPtrTraits<T1>::unwrap(dest), TVirtPtrTraits<T2>::unwrap(src), size);
        return dest;
    }

#ifdef VIRTMEM_WRAP_CPOINTERS
    if (TVirtPtrTraits<T1>::isWrapped(dest) && TVirtPtrTraits<T2>::isWrapped(src))
    {
        copier(TVirtPtrTraits<T1>::unwrap(dest), TVirtPtrTraits<T2>::unwrap(src), size);
        return dest;
    }
    else if (TVirtPtrTraits<T1>::isWrapped(dest))
    {
        rawCopy(TVirtPtrTraits<T1>::unwrap(dest), src, size, copier, checknull);
        return dest;
    }
    else if (TVirtPtrTraits<T2>::isWrapped(src))
        return rawCopy(dest, TVirtPtrTraits<T2>::unwrap(src), size, copier, checknull);
#endif

    /* if both are virtual pointers and are same type: both must be locked, check for two
     * if both are virtual pointers and are inequal type: lock both, check both allocators if one slot available
     * if one is virtual pointer, check if it can be locked */

    // UNDONE: also try smaller locks if no free big pages are available?

    bool uselock = false;
    TVirtPageSize locksize = min(TVirtPtrTraits<T1>::getPageSize(), TVirtPtrTraits<T2>::getPageSize());

    if (TSameType<T1, T2>::flag && TVirtPtrTraits<T1>::isVirtPtr())
    {
        uselock = (TVirtPtrTraits<T1>::getUnlockedBigPages(dest) >= 2);
        // make sure we don't overlap locks
        if (uselock)
            locksize = min(locksize, static_cast<TVirtPageSize>(abs(ptrDiff(dest, src))));
    }
    else if (TVirtPtrTraits<T1>::isVirtPtr() && TVirtPtrTraits<T2>::isVirtPtr())
        uselock = (TVirtPtrTraits<T1>::getUnlockedBigPages(dest) >= 1) &&
                  (TVirtPtrTraits<T2>::getUnlockedBigPages(src) >= 1);
    else if (TVirtPtrTraits<T1>::isVirtPtr())
        uselock = (TVirtPtrTraits<T1>::getUnlockedBigPages(dest) >= 1);
    else if (TVirtPtrTraits<T2>::isVirtPtr())
        uselock = (TVirtPtrTraits<T2>::getUnlockedBigPages(src) >= 1);

    TVirtPtrSize sizeleft = size;
    T1 p1 = dest;
    T2 p2 = src;

    if (uselock)
    {
        while (sizeleft)
        {
            TVirtPageSize cpsize = min(static_cast<TVirtPtrSize>(locksize), sizeleft);
            // NOTE: cpsize might be reduced after lock is constructed
            typename TVirtPtrTraits<T1>::TLock l1 = TVirtPtrTraits<T1>::makeLock(p1, cpsize, true);
            typename TVirtPtrTraits<T2>::TLock l2 = TVirtPtrTraits<T2>::makeLock(p2, cpsize, true, true);
            if (!copier(*l1, *l2, cpsize))
                break;

            p1 += cpsize; p2 += cpsize;
            sizeleft -= cpsize;
            ASSERT(sizeleft <= size);
        }
    }
    else
    {
        // slow fallback for when no locks can be used
        for (; sizeleft; ++p1, ++p2, --sizeleft)
        {
            *p1 = (typename TVirtPtrTraits<T2>::TValue)(*p2);
            if (checknull && *p1 == 0)
                break;
        }
    }

    return dest;
}

// Generalized compare for memcmp/strncmp
template <typename T1, typename T2> int rawCompare(T1 s1, T2 s2, TVirtPtrSize n, bool checknull)
{
    if (n == 0)
        return 0;

    typename private_utils::TAntiConst<typename private_utils::TVirtPtrTraits<T1>::TValue>::type v1;
    typename private_utils::TAntiConst<typename private_utils::TVirtPtrTraits<T2>::TValue>::type v2;
    for (TVirtPtrSize i=0; i<n; ++i, ++s1, ++s2)
    {
        v1 = *s1; v2 = *s2; // having local copies makes things much faster for virt pointers
        if (v1 < v2)
            return -1;
        if (v1 > v2)
            return 1;
        if (checknull && v1 == 0 /*|| *s2 == 0*/) // if we are here, both must be the same
            break;
    }

    return 0;
}

inline bool memCopier(char *dest, const char *src, TVirtPtrSize n)
{
    memcpy(dest, src, n);
    return true;
}

inline bool strnCopier(char *dest, const char *src, TVirtPtrSize n)
{
    return n && (strncpy(dest, src, n))[n-1] != 0; // strncpy pads with zero
}

inline bool strCopier(char *dest, const char *src, TVirtPtrSize n)
{
    // can't use strcpy since it doesn't take into account specified size, cannot take
    // strncpy either since it pads and since we don't actually know size of dest
    // it could pad too much... well could you just use something else then strcpy?

    if (n == 0)
        return false;

    char *d = dest;
    const char *s = src;
    for (; n; --n, ++d, ++s)
    {
        *d = *s;
        if (*d == 0)
            return false; // abort if 0 found
    }

    return true;
}

}


// Specialize memcpy/memset/memcmp for char types: we may have to convert to this type anyway,
// and this saves some template code duplication for other types. A general template function will just
// cast and call the specialized function.

template <typename T1, typename A1, typename T2, typename A2>
CVirtPtr<T1, A1> memcpy(CVirtPtr<T1, A1> dest, const CVirtPtr<T2, A2> src, TVirtPtrSize size)
{
    return static_cast<CVirtPtr<T1, A1> >(
                private_utils::rawCopy(static_cast<CVirtPtr<char, A1> >(dest),
                                       static_cast<const CVirtPtr<const char, A2> >(src), size,
                                       private_utils::memCopier, false));
}

template <typename T, typename A> CVirtPtr<T, A> memcpy(CVirtPtr<T, A> dest, const void *src, TVirtPtrSize size)
{
    return static_cast<CVirtPtr<T, A> >(
                private_utils::rawCopy(static_cast<CVirtPtr<char, A> >(dest),
                                       static_cast<const char *>(src), size,
                                       private_utils::memCopier, false));
}

template <typename T, typename A> void *memcpy(void *dest, CVirtPtr<T, A> src, TVirtPtrSize size)
{
    return private_utils::rawCopy(static_cast<char *>(dest),
                                  static_cast<const CVirtPtr<const char, A> >(src), size,
                                  private_utils::memCopier, false);
}

template <typename A> CVirtPtr<char, A> memset(CVirtPtr<char, A> dest, int c, TVirtPtrSize size)
{
    if (size == 0)
        return dest;

#ifdef VIRTMEM_WRAP_CPOINTERS
    if (dest.isWrapped())
    {
        memset(dest.unwrap(), c, size);
        return dest;
    }
#endif

    CVirtPtr<char, A> p = dest;
    TVirtPtrSize sizeleft = size;

    if (A::getInstance()->getUnlockedBigPages() >= 1)
    {
        while (sizeleft)
        {
            TVirtPageSize setsize = private_utils::min(A::getInstance()->getBigPageSize(), sizeleft);
            memset(*makeVirtPtrLock(p, setsize, true), c, setsize);
            p += setsize; sizeleft -= setsize;
        }
    }
    else
    {
        // slow fallback for when no locks can be used
        for (; sizeleft; ++p, --sizeleft)
            *p = c;
    }

    return dest;
}

template <typename T, typename A> CVirtPtr<T, A> memset(CVirtPtr<T, A> dest, int c, TVirtPtrSize size)
{
    return static_cast<CVirtPtr<T, A> >(memset(static_cast<CVirtPtr<char, A> >(dest), c, size));
}

template <typename T1, typename A1, typename T2, typename A2> int memcmp(CVirtPtr<T1, A1> s1, const CVirtPtr<T2, A2> s2,
                                                                         TVirtPtrSize n)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (s1.isWrapped() && s2.isWrapped())
        return memcmp(s1.unwrap(), s2.unwrap(), n);
#endif

    return private_utils::rawCompare(static_cast<CVirtPtr<const char, A1> >(s1),
                                     static_cast<CVirtPtr<const char, A2> >(s2), n, false);
}

template <typename T, typename A> int memcmp(CVirtPtr<T, A> s1, const void *s2, TVirtPtrSize n)
{
    return private_utils::rawCompare(static_cast<CVirtPtr<const char, A> >(s1),
                                     static_cast<const char *>(s2), n, false);
}

template <typename T, typename A> int memcmp(const void *s1, const CVirtPtr<T, A> s2, TVirtPtrSize n)
{
    return private_utils::rawCompare(static_cast<const char *>(s1),
                                     static_cast<CVirtPtr<const char, A> >(s2), n, false);
}

template <typename A1, typename A2> CVirtPtr<char, A1> strncpy(CVirtPtr<char, A1> dest, const CVirtPtr<const char, A2> src,
                                                              TVirtPtrSize n)
{
    return private_utils::rawCopy(dest, src, n, private_utils::strnCopier, true);
}

template <typename A> CVirtPtr<char, A> strncpy(CVirtPtr<char, A> dest, const char *src, TVirtPtrSize n)
{
    return private_utils::rawCopy(dest, src, n, private_utils::strnCopier, true);
}

template <typename A> char *strncpy(char *dest, const CVirtPtr<const char, A> src, TVirtPtrSize n)
{
    return private_utils::rawCopy(dest, src, n, private_utils::strnCopier, true);
}

template <typename A1, typename A2> CVirtPtr<char, A1> strcpy(CVirtPtr<char, A1> dest, const CVirtPtr<const char, A2> src)
{
    return private_utils::rawCopy(dest, src, (TVirtPtrSize)-1, private_utils::strCopier, true);
}

template <typename A> CVirtPtr<char, A> strcpy(CVirtPtr<char, A> dest, const char *src)
{
    return private_utils::rawCopy(dest, src, (TVirtPtrSize)-1, private_utils::strCopier, true);
}

template <typename A> char *strcpy(char *dest, const CVirtPtr<const char, A> src)
{
    return private_utils::rawCopy(dest, src, (TVirtPtrSize)-1, private_utils::strCopier, true);
}

template <typename A1, typename A2> int strncmp(CVirtPtr<const char, A1> dest, CVirtPtr<const char, A2> src, TVirtPtrSize n)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (dest.isWrapped() && src.isWrapped())
        return strncmp(dest.unwrap(), src.unwrap(), n);
#endif
    return private_utils::rawCompare(dest, src, n, true);
}

template <typename A> int strncmp(CVirtPtr<const char, A> dest, const char *src, TVirtPtrSize n)
{ return private_utils::rawCompare(dest, src, n, true); }
template <typename A> int strncmp(const char *dest, CVirtPtr<const char, A> src, TVirtPtrSize n)
{ return private_utils::rawCompare(dest, src, n, true); }

template <typename A1, typename A2> int strcmp(CVirtPtr<const char, A1> dest, CVirtPtr<const char, A2> src)
{ return strncmp(dest, src, (TVirtPtrSize)-1); }
template <typename A> int strcmp(const char *dest, CVirtPtr<const char, A> src)
{ return strncmp(dest, src, (TVirtPtrSize)-1); }
template <typename A> int strcmp(CVirtPtr<const char, A> dest, const char *src)
{ return strncmp(dest, src, (TVirtPtrSize)-1); }

template <typename A> int strlen(CVirtPtr<const char, A> str)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (str.isWrapped())
        return strlen(str.unwrap());
#endif

    int ret = 0;
    for (; *str != 0; ++str)
        ++ret;
    return ret;
}

// const <--> non const madness
// -----

template <typename A1, typename A2> CVirtPtr<char, A1> strncpy(CVirtPtr<char, A1> dest, const CVirtPtr<char, A2> src,
                                                              TVirtPtrSize n)
{ return strncpy(dest, static_cast<const CVirtPtr<const char, A2> >(src), n); }
template <typename A> char *strncpy(char *dest, const CVirtPtr<char, A> src, TVirtPtrSize n)
{ return strncpy(dest, static_cast<const CVirtPtr<const char, A> >(src), n); }

template <typename A1, typename A2> CVirtPtr<char, A1> strcpy(CVirtPtr<char, A1> dest, const CVirtPtr<char, A2> src)
{ return strcpy(dest, static_cast<const CVirtPtr<const char, A2> >(src)); }
template <typename A> char *strcpy(char *dest, const CVirtPtr<char, A> src)
{ return strcpy(dest, static_cast<const CVirtPtr<const char, A> >(src)); }

template <typename A1, typename A2> int strncmp(CVirtPtr<char, A1> dest, CVirtPtr<char, A2> src, TVirtPtrSize n)
{ return strncmp(static_cast<CVirtPtr<const char, A1> >(dest), static_cast<CVirtPtr<const char, A2> >(src), n); }
template <typename A1, typename A2> int strncmp(CVirtPtr<const char, A1> dest, CVirtPtr<char, A2> src, TVirtPtrSize n)
{ return strncmp(dest, static_cast<CVirtPtr<const char, A2> >(src), n); }
template <typename A1, typename A2> int strncmp(CVirtPtr<char, A1> dest, CVirtPtr<const char, A2> src, TVirtPtrSize n)
{ return strncmp(static_cast<CVirtPtr<const char, A1> >(dest), src, n); }
template <typename A> int strncmp(const char *dest, CVirtPtr<char, A> src, TVirtPtrSize n)
{ return strncmp(dest, static_cast<CVirtPtr<const char, A> >(src), n); }
template <typename A> int strncmp(CVirtPtr<char, A> dest, const char *src, TVirtPtrSize n)
{ return strncmp(static_cast<CVirtPtr<const char, A> >(dest), src, n); }

template <typename A1, typename A2> int strcmp(CVirtPtr<char, A1> dest, CVirtPtr<char, A2> src)
{ return strcmp(static_cast<CVirtPtr<const char, A1> >(dest), static_cast<CVirtPtr<const char, A2> >(src)); }
template <typename A1, typename A2> int strcmp(CVirtPtr<const char, A1> dest, CVirtPtr<char, A2> src)
{ return strcmp(dest, static_cast<CVirtPtr<const char, A2> >(src)); }
template <typename A1, typename A2> int strcmp(CVirtPtr<char, A1> dest, CVirtPtr<const char, A2> src)
{ return strcmp(static_cast<CVirtPtr<const char, A1> >(dest), src); }
template <typename A> int strcmp(const char *dest, CVirtPtr<char, A> src)
{ return strcmp(dest, static_cast<CVirtPtr<const char, A> >(src)); }
template <typename A> int strcmp(CVirtPtr<char, A> dest, const char *src)
{ return strcmp(static_cast<CVirtPtr<const char, A> >(dest), src); }

template <typename A> int strlen(CVirtPtr<char, A> str) { return strlen(static_cast<CVirtPtr<const char, A> >(str)); }

#endif // UTILS_HPP

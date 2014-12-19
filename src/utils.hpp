#ifndef UTILS_HPP
#define UTILS_HPP

#include "config.h"

#include <string.h>

namespace private_utils {

template <typename T, typename T2> struct TSameType { static const bool flag = false; };
template <typename T> struct TSameType<T, T> { static const bool flag = true; };

template <typename T> struct TVirtPtrTraits { };

template <typename T, typename A> struct TVirtPtrTraits<CVirtPtr<T, A> >
{
    static bool isWrapped(CVirtPtr<T, A> p) { return p.isWrapped(); }
    static T *unwrap(CVirtPtr<T, A> p) { return p.unwrap(); }
    static bool isVirtPtr(void) { return true; }
    static CPtrWrapLock<CVirtPtr<T, A> > makeLock(const CVirtPtr<T, A> &w, bool ro=false)
    { return makePtrWrapLock(w, ro); }
    static uint8_t getUnlockedPages(CVirtPtr<T, A>)
    { return A::getInstance()->getUnlockedPages(); }
    static TVirtPtrSize getPageSize(CVirtPtr<T, A>)
    { return A::getInstance()->getPageSize(); }
};

template <typename T> struct TVirtPtrTraits<T *>
{
    static bool isWrapped(T *) { return false; }
    static T *unwrap(T *p) { return (T *)p; }
    static bool isVirtPtr(void) { return false; }
    static T **makeLock(T *&p, __attribute__ ((unused)) bool ro=false) { return &p; }
    static uint8_t getUnlockedPages(const T *) { return 0; }
    static TVirtPtrSize getPageSize(const T *) { return (TVirtPtrSize)-1; }
};

template <typename T> T min(const T &v1, const T &v2) { return (v1 < v2) ? v1 : v2; }

// copier function that works with raw pointers for rawCopy, return false if copying should be aborted
typedef bool (*TRawCopier)(char *, const char *, TVirtPtrSize);

// Generalized copy for memcpy and strncpy
template <typename T1, typename T2> T1 rawCopy(T1 dest, T2 src, TVirtPtrSize size,
                                               TRawCopier copier, bool checknull)
{
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

    bool uselock = false;

    if (TSameType<T1, T2>::flag && TVirtPtrTraits<T1>::isVirtPtr())
        uselock = (TVirtPtrTraits<T1>::getUnlockedPages(dest) >= 2);
    else if (TVirtPtrTraits<T1>::isVirtPtr() && TVirtPtrTraits<T2>::isVirtPtr())
        uselock = (TVirtPtrTraits<T1>::getUnlockedPages(dest) >= 1) &&
                  (TVirtPtrTraits<T2>::getUnlockedPages(src) >= 1);
    else if (TVirtPtrTraits<T1>::isVirtPtr())
        uselock = (TVirtPtrTraits<T1>::getUnlockedPages(dest) >= 1);
    else if (TVirtPtrTraits<T2>::isVirtPtr())
        uselock = (TVirtPtrTraits<T2>::getUnlockedPages(src) >= 1);

    if (uselock)
    {
        TVirtPtrSize start = 0;
        const TVirtPtrSize psize = min(TVirtPtrTraits<T1>::getPageSize(dest),
                                       TVirtPtrTraits<T2>::getPageSize(src));
        while (start < size)
        {
            const TVirtPtrSize cpsize = (start + psize) < size ? psize : (size-start);
            T1 tmpdest = dest + start;
            T2 tmpsrc = src + start;
            if (!copier(*TVirtPtrTraits<T1>::makeLock(tmpdest),
                        *TVirtPtrTraits<T2>::makeLock(tmpsrc, true), cpsize))
                break;
            start += psize;
        }
    }
    else
    {
        for (TVirtPtrSize s=0; s<size; ++s)
        {
            dest[s] = src[s];
            if (/*(dest[s] = src[s])*/dest[s] == 0 && checknull)
                break;
        }
    }

    return dest;
}

// Generalized compare for memcmp/strncmp
template <typename T1, typename T2> int rawCompare(T1 s1, T2 s2, TVirtPtrSize n, bool checknull)
{
    for (TVirtPtrSize i=0; i<n; ++i, ++s1, ++s2)
    {
        if (*s1 < *s2)
            return -1;
        if (*s1 > *s2)
            return 1;
        if (checknull && *s1 == 0 /*|| *s2 == 0*/) // if we are here, both must be the same
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
    return (strncpy(dest, src, n))[n-1] != 0; // strncpy pads with zero
}

inline bool strCopier(char *dest, const char *src, TVirtPtrSize n)
{
    strcpy(dest, src);

    // check if we have a zero
    for (; *dest && n; ++dest)
        ;

    return n == 0;
}

}


// Specialize memcpy/memset/memcmp for char types: we may have to convert to this type anyway,
// and this saves some template code duplication for other types. A general template function will just
// cast and call the specialized function.

template <typename T, typename A> CVirtPtr<T, A> memcpy(CVirtPtr<T, A> dest, const CVirtPtr<T, A> src,
                                                        TVirtPtrSize size)
{
    return static_cast<CVirtPtr<T, A> >(
                private_utils::rawCopy(static_cast<CVirtPtr<char, A> >(dest),
                                       static_cast<const CVirtPtr<const char, A> >(src), size,
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
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (dest.isWrapped())
    {
        memset(dest.unwrap(), c, size);
        return dest;
    }
#endif

    // If dest can be locked, use regular memset
    if (A::getInstance()->getUnlockedPages() >= 1)
    {
        TVirtPtrSize start = 0;
        const TVirtPtrSize psize = A::getInstance()->getPageSize();
        while (start < size)
        {
            const TVirtPtrSize cpsize = (start + psize) < size ? psize : (size-start);
            memset(*makePtrWrapLock(dest + start), c, cpsize);
            start += psize;
        }
    }
    else
    {
        for (TVirtPtrSize s=0; s<size; ++s)
            dest[s] = c;
    }

    return dest;
}

template <typename T, typename A> CVirtPtr<T, A> memset(CVirtPtr<T, A> dest, int c, TVirtPtrSize size)
{
    return static_cast<CVirtPtr<T, A> >(memset(static_cast<CVirtPtr<char, A> >(dest), c, size));
}

template <typename T, typename A> int memcmp(CVirtPtr<T, A> s1, const CVirtPtr<T, A> s2, TVirtPtrSize n)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (s1.isWrapped() && s2.isWrapped())
        return memcmp(s1.unwrap(), s2.unwrap(), n);
#endif

    return private_utils::rawCompare(static_cast<CVirtPtr<char, A> >(s1),
                                     static_cast<CVirtPtr<char, A> >(s2), n, false);
}

template <typename T, typename A> int memcmp(CVirtPtr<T, A> s1, const void *s2, TVirtPtrSize n)
{
    return private_utils::rawCompare(static_cast<CVirtPtr<char, A> >(s1),
                                     static_cast<const char *>(s2), n, false);
}

template <typename T, typename A> int memcmp(const void *s1, const CVirtPtr<T, A> s2, TVirtPtrSize n)
{
    return private_utils::rawCompare(static_cast<const char *>(s1),
                                     static_cast<CVirtPtr<char, A> >(s2), n, false);
}

template <typename A> CVirtPtr<char, A> strncpy(CVirtPtr<char, A> dest, const CVirtPtr<char, A> src,
                                                TVirtPtrSize n)
{
    return private_utils::rawCopy(dest, static_cast<const CVirtPtr<const char, A> >(src), n,
                                  private_utils::strnCopier, true);
}

template <typename A> CVirtPtr<char, A> strncpy(CVirtPtr<char, A> dest, const char *src, TVirtPtrSize n)
{
    return private_utils::rawCopy(dest, src, n, private_utils::strnCopier, true);
}

template <typename A> char *strncpy(char *dest, const CVirtPtr<char, A> src, TVirtPtrSize n)
{
    return private_utils::rawCopy(dest, static_cast<const CVirtPtr<const char, A> >(src), n,
                                  private_utils::strnCopier, true);
}

template <typename A> CVirtPtr<char, A> strcpy(CVirtPtr<char, A> dest, const CVirtPtr<char, A> src)
{
    return private_utils::rawCopy(dest, static_cast<const CVirtPtr<const char, A> >(src),
                                  (TVirtPtrSize)-1, private_utils::strCopier, true);
}

template <typename A> CVirtPtr<char, A> strcpy(CVirtPtr<char, A> dest, const char *src)
{
    return private_utils::rawCopy(dest, src, (TVirtPtrSize)-1, private_utils::strCopier, true);
}

template <typename A> char *strcpy(char *dest, const CVirtPtr<char, A> src)
{
    return private_utils::rawCopy(dest, static_cast<const CVirtPtr<const char, A> >(src),
                                  (TVirtPtrSize)-1, private_utils::strCopier, true);
}

template <typename A> int strncmp(CVirtPtr<char, A> dest, CVirtPtr<char, A> src, TVirtPtrSize n)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (dest.isWrapped() && src.isWrapped())
        return strncmp(dest.unwrap(), src.unwrap(), n);
#endif
    return private_utils::rawCompare(dest, src, n, true);
}

template <typename A> int strncmp(CVirtPtr<char, A> dest, const char *src, TVirtPtrSize n)
{ return private_utils::rawCompare(dest, src, n, true); }
template <typename A> int strncmp(const char *dest, CVirtPtr<char, A> src, TVirtPtrSize n)
{ return private_utils::rawCompare(dest, src, n, true); }

template <typename A> int strcmp(CVirtPtr<char, A> dest, CVirtPtr<char, A> src)
{ return strncmp(dest, src, (TVirtPtrSize)-1); }
template <typename A> int strcmp(const char *dest, CVirtPtr<char, A> src)
{ return strncmp(dest, src, (TVirtPtrSize)-1); }
template <typename A> int strcmp(CVirtPtr<char, A> dest, const char *src)
{ return strncmp(dest, src, (TVirtPtrSize)-1); }

template <typename A> int strlen(CVirtPtr<char, A> str)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (str.isWrapped())
        return strlen(str.unwrap());
#endif

    int ret = 0;
    for (; *str; ++str)
        ++ret;
    return ret;
}

#endif // UTILS_HPP

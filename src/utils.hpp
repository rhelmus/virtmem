#ifndef VIRTMEM_UTILS_HPP
#define VIRTMEM_UTILS_HPP

#include "config.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

//#include <stdio.h> // UNDONE

// Arduino define min/max macros, temporarily get rid of them.
// They are restored at the end of this file
#ifdef min
#pragma push_macro("min")
#undef min
#endif

#ifdef max
#undef max
#pragma push_macro("max")
#endif

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
    static TLock makeLock(CVirtPtr<T, A> w, TVirtPageSize &s, bool ro=false)
    { return makeVirtPtrLock(w, s, ro); }
    static uint8_t getUnlockedBigPages(void)
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
    static TLock makeLock(T *&p, TVirtPageSize &, __attribute__ ((unused)) bool ro=false) { return &p; }
    static uint8_t getUnlockedBigPages(void) { return 0; }
    static TVirtPageSize getPageSize(void) { return (TVirtPageSize)-1; }
};

template <typename T1, typename T2, typename A> int ptrDiff(CVirtPtr<T1, A> p1, CVirtPtr<T2, A> p2) { return p2 - p1; }
template <typename T1, typename T2> int ptrDiff(T1 *p1, T2 *p2) { return p2 - p1; }
template <typename T1, typename T2> int ptrDiff(T1, T2) { return 0; } // dummy, not used (for mix of regular and virt pointers)
template <typename T1, typename T2, typename A> bool ptrEqual(CVirtPtr<T1, A> p1, CVirtPtr<T2, A> p2) { return p1 == p2; }
template <typename T1, typename T2> bool ptrEqual(T1 *p1, T2 *p2) { return p1 == p2; }
template <typename T1, typename T2> bool ptrEqual(T1, T2) { return false; } // mix of virt and regular pointers

#if 0
template <typename T1, typename T2> bool canUseLocks(T1 p1, T2 p2, TVirtPageSize &size)
{
    // UNDONE: also try smaller locks if no free big pages are available?

    bool ret = false;
    size = min(TVirtPtrTraits<T1>::getPageSize(), TVirtPtrTraits<T2>::getPageSize());

    /* if both are virtual pointers and are same type: both must be locked, check for two
     * if both are virtual pointers and are inequal type: lock both, check both allocators if one slot available
     * if one is virtual pointer, check if it can be locked */

    if (TSameType<T1, T2>::flag && TVirtPtrTraits<T1>::isVirtPtr())
    {
        ret = (TVirtPtrTraits<T1>::getUnlockedBigPages() >= 2);
        // make sure we don't overlap locks
        if (ret)
            size = min(size, static_cast<TVirtPageSize>(abs(ptrDiff(p1, p2))));
    }
    else if (TVirtPtrTraits<T1>::isVirtPtr() && TVirtPtrTraits<T2>::isVirtPtr())
        ret = (TVirtPtrTraits<T1>::getUnlockedBigPages() >= 1) &&
                (TVirtPtrTraits<T2>::getUnlockedBigPages() >= 1);
    else if (TVirtPtrTraits<T1>::isVirtPtr())
        ret = (TVirtPtrTraits<T1>::getUnlockedBigPages() >= 1);
    else if (TVirtPtrTraits<T2>::isVirtPtr())
        ret = (TVirtPtrTraits<T2>::getUnlockedBigPages() >= 1);

    return ret && size > 0;
}
#endif

template <typename T1, typename T2> TVirtPageSize getMaxLockSize(T1 p1, T2 p2)
{
    TVirtPageSize ret = min(TVirtPtrTraits<T1>::getPageSize(), TVirtPtrTraits<T2>::getPageSize());

    // check for overlap in case both p1 and p2 are virtual pointers from the same allocator
    if (TSameType<T1, T2>::flag && TVirtPtrTraits<T1>::isVirtPtr())
        ret = min(ret, static_cast<TVirtPageSize>(abs(ptrDiff(p1, p2))));

    return ret;
}


// copier function that works with raw pointers for rawCopy, returns false if copying should be aborted
typedef bool (*TRawCopier)(char *, const char *, TVirtPtrSize);

// Generalized copy for memcpy and strncpy
template <typename T1, typename T2> T1 rawCopy(T1 dest, T2 src, TVirtPtrSize size,
                                               TRawCopier copier, bool checknull)
{
    if (size == 0 || ptrEqual(dest, src))
        return dest;

#ifdef VIRTMEM_WRAP_CPOINTERS
    if (!TVirtPtrTraits<T1>::isVirtPtr() && !TVirtPtrTraits<T2>::isVirtPtr())
    {
        // Even though both are pointers, we must unwrap to avoid compile errors with other types
        copier(TVirtPtrTraits<T1>::unwrap(dest), TVirtPtrTraits<T2>::unwrap(src), size);
        return dest;
    }
    else if (TVirtPtrTraits<T1>::isWrapped(dest) && TVirtPtrTraits<T2>::isWrapped(src))
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

    TVirtPtrSize sizeleft = size;
    const TVirtPageSize maxlocksize = getMaxLockSize(dest, src);
    T1 p1 = dest;
    T2 p2 = src;

    /* if both can be locked, copy max maxlocksize bytes with lock
     * else if maxlocksize < 2 copy everything without locks
     * else copy maxlocksize bytes without locks */

    while (sizeleft)
    {
        TVirtPtrSize plaincpsize;

        if (maxlocksize < 2)
            plaincpsize = sizeleft;
        else
        {
            TVirtPageSize cpsize = min(static_cast<TVirtPtrSize>(maxlocksize), sizeleft);
            plaincpsize = cpsize; // will be reset to zero in case locks can be used

            // NOTE: cpsize might be reduced after lock is constructed
            typename TVirtPtrTraits<T1>::TLock l1 = TVirtPtrTraits<T1>::makeLock(p1, cpsize);
            if (l1)
            {
                typename TVirtPtrTraits<T2>::TLock l2 = TVirtPtrTraits<T2>::makeLock(p2, cpsize, true);
                if (l2)
                {
                    if (!copier(*l1, *l2, cpsize))
                        return dest;

                    p1 += cpsize; p2 += cpsize;
                    sizeleft -= cpsize;
                    ASSERT(sizeleft <= size);
                    plaincpsize = 0;
                }
            }
        }

        // slow fallback for when no locks can be used
        if (plaincpsize != 0)
        {
            sizeleft -= plaincpsize;
            for (; plaincpsize; ++p1, ++p2, --plaincpsize)
            {
                *p1 = (typename TVirtPtrTraits<T2>::TValue)(*p2);
                if (checknull && *p1 == 0)
                    return dest;
            }
        }
    }

#if 0
    TVirtPtrSize sizeleft = size;
    TVirtPageSize locksize; // filled in by canUseLocks()
    T1 p1 = dest;
    T2 p2 = src;

    if (canUseLocks(dest, src, locksize))
    {
        while (sizeleft)
        {
            TVirtPageSize cpsize = min(static_cast<TVirtPtrSize>(locksize), sizeleft);
            // NOTE: cpsize might be reduced after lock is constructed
            typename TVirtPtrTraits<T1>::TLock l1 = TVirtPtrTraits<T1>::makeLock(p1, cpsize);
            typename TVirtPtrTraits<T2>::TLock l2 = TVirtPtrTraits<T2>::makeLock(p2, cpsize, true);
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
#endif

    return dest;
}

// comparison function for rawCompare
typedef int (*TRawComparator)(const char *, const char *, TVirtPtrSize, bool &);

// Generalized compare for memcmp/strncmp
template <typename T1, typename T2> int rawCompare(T1 p1, T2 p2, TVirtPtrSize n, TRawComparator comparator,
                                                   bool checknull)
{
    if (n == 0 || ptrEqual(p1, p2))
        return 0;

    bool done = false;

#ifdef VIRTMEM_WRAP_CPOINTERS
    if (!TVirtPtrTraits<T1>::isVirtPtr() && !TVirtPtrTraits<T2>::isVirtPtr())
    {
        // Even though both are pointers, we must unwrap to avoid compile errors with other types
        return comparator(TVirtPtrTraits<T1>::unwrap(p1), TVirtPtrTraits<T2>::unwrap(p2), n, done);
    }
    else if (TVirtPtrTraits<T1>::isWrapped(p1) && TVirtPtrTraits<T2>::isWrapped(p2))
        return comparator(TVirtPtrTraits<T1>::unwrap(p1), TVirtPtrTraits<T2>::unwrap(p2), n, done);
    else if (TVirtPtrTraits<T1>::isWrapped(p1))
        return rawCompare(TVirtPtrTraits<T1>::unwrap(p1), p2, n, comparator, checknull);
    else if (TVirtPtrTraits<T2>::isWrapped(p2))
        return rawCompare(p1, TVirtPtrTraits<T2>::unwrap(p2), n, comparator, checknull);
#endif

    TVirtPtrSize sizeleft = n;
    const TVirtPageSize maxlocksize = getMaxLockSize(p1, p2);

    /* if both can be locked, compare max maxlocksize bytes with lock
     * else if maxlocksize < 2 compare everything without locks
     * else compare maxlocksize bytes without locks */

    while (sizeleft)
    {
        TVirtPtrSize plaincmpsize;

        if (maxlocksize < 2)
            plaincmpsize = sizeleft;
        else
        {
            TVirtPageSize cmpsize = min(static_cast<TVirtPtrSize>(maxlocksize), sizeleft);
            plaincmpsize = cmpsize; // will be reset to zero in case locks can be used

            // NOTE: cpsize might be reduced after lock is constructed
            typename TVirtPtrTraits<T1>::TLock l1 = TVirtPtrTraits<T1>::makeLock(p1, cmpsize, true);
            if (l1)
            {
                typename TVirtPtrTraits<T2>::TLock l2 = TVirtPtrTraits<T2>::makeLock(p2, cmpsize, true);
                if (l2)
                {
                    int cmp = comparator(*l1, *l2, cmpsize, done);
                    if (cmp != 0 || done)
                        return cmp;

                    p1 += cmpsize; p2 += cmpsize;
                    sizeleft -= cmpsize;
                    ASSERT(sizeleft <= n);
                    plaincmpsize = 0;
                }
            }
        }

        // slow fallback for when no locks can be used
        if (plaincmpsize != 0)
        {
            sizeleft -= plaincmpsize;
            typename private_utils::TAntiConst<typename private_utils::TVirtPtrTraits<T1>::TValue>::type v1;
            typename private_utils::TAntiConst<typename private_utils::TVirtPtrTraits<T2>::TValue>::type v2;
            for (; plaincmpsize; ++p1, ++p2, --plaincmpsize)
            {
                v1 = *p1; v2 = *p2; // having local copies makes things much faster for virt pointers
                if (v1 < v2)
                    return -1;
                if (v1 > v2)
                    return 1;
                if (checknull && v1 == 0 /*|| v2 == 0*/) // if we are here, both must be the same
                    return 0;
            }
        }
    }

#if 0
    TVirtPageSize locksize; // filled in by canUseLocks()
    if (canUseLocks(p1, p2, locksize))
    {
        TVirtPtrSize sizeleft = n;
        while (sizeleft)
        {
            TVirtPageSize cmpsize = min(static_cast<TVirtPtrSize>(locksize), sizeleft);
            // NOTE: cpsize might be reduced after lock is constructed
            typename TVirtPtrTraits<T1>::TLock l1 = TVirtPtrTraits<T1>::makeLock(p1, cmpsize, true);
            typename TVirtPtrTraits<T2>::TLock l2 = TVirtPtrTraits<T2>::makeLock(p2, cmpsize, true);

            int cmp = comparator(*l1, *l2, cmpsize, done);
            if (cmp != 0 || done)
                return cmp;

            p1 += cmpsize; p2 += cmpsize;
            sizeleft -= cmpsize;
            ASSERT(sizeleft <= n);
        }
    }
    else
    {
        typename private_utils::TAntiConst<typename private_utils::TVirtPtrTraits<T1>::TValue>::type v1;
        typename private_utils::TAntiConst<typename private_utils::TVirtPtrTraits<T2>::TValue>::type v2;
        for (TVirtPtrSize i=0; i<n; ++i, ++p1, ++p2)
        {
            v1 = *p1; v2 = *p2; // having local copies makes things much faster for virt pointers
            if (v1 < v2)
                return -1;
            if (v1 > v2)
                return 1;
            if (checknull && v1 == 0 /*|| v2 == 0*/) // if we are here, both must be the same
                break;
        }
    }
#endif

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

inline int memComparator(const char *p1, const char *p2, TVirtPtrSize n, bool &)
{ return ::memcmp(p1, p2, n); }
inline int strnComparator(const char *p1, const char *p2, TVirtPtrSize n, bool &)
{ return ::strncmp(p1, p2, n); }
inline int strComparator(const char *p1, const char *p2, TVirtPtrSize n, bool &done)
{
    const int ret = ::strncmp(p1, p2, n);
    if (ret == 0)
    {
        // did we encounter a zero?
        for (; n && *p1; --n, ++p1)
            ;
        done = (n != 0); // we're done if there was a string terminator
    }
    return  ret;
}

}

namespace virtmem {

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
        ::memset(dest.unwrap(), c, size);
        return dest;
    }
#endif

    TVirtPtrSize sizeleft = size;
    CVirtPtr<char, A> p = dest;

    /* if both can be locked, copy max maxlocksize bytes with lock
     * else if maxlocksize < 2 copy everything without locks
     * else copy maxlocksize bytes without locks */

    while (sizeleft)
    {
        TVirtPageSize setsize = private_utils::min(A::getInstance()->getBigPageSize(), sizeleft);

        // NOTE: setsize might be reduced after lock is constructed
        CPtrWrapLock<CVirtPtr<char, A> > l = makeVirtPtrLock(p, setsize);
        if (l)
        {
            ::memset(*l, c, setsize);
            p += setsize; sizeleft -= setsize;
        }
        else
        {
            // slow fallback for when no locks can be used
            sizeleft -= setsize;
            for (; setsize; ++p, --setsize)
                *p = c;
        }
    }

#if 0
    CVirtPtr<char, A> p = dest;
    TVirtPtrSize sizeleft = size;

    if (A::getInstance()->getUnlockedBigPages() >= 1)
    {
        while (sizeleft)
        {
            TVirtPageSize setsize = private_utils::min(A::getInstance()->getBigPageSize(), sizeleft);
            CPtrWrapLock<CVirtPtr<char, A> > l = makeVirtPtrLock(p, setsize);
            ::memset(*l, c, setsize);
            p += setsize; sizeleft -= setsize;
        }
    }
    else
    {
        // slow fallback for when no locks can be used
        for (; sizeleft; ++p, --sizeleft)
            *p = c;
    }
#endif

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
        return ::memcmp(s1.unwrap(), s2.unwrap(), n);
#endif

    return private_utils::rawCompare(static_cast<CVirtPtr<const char, A1> >(s1),
                                     static_cast<CVirtPtr<const char, A2> >(s2), n, private_utils::memComparator, false);
}

template <typename T, typename A> int memcmp(CVirtPtr<T, A> s1, const void *s2, TVirtPtrSize n)
{
    return private_utils::rawCompare(static_cast<CVirtPtr<const char, A> >(s1),
                                     static_cast<const char *>(s2), n, private_utils::memComparator, false);
}

template <typename T, typename A> int memcmp(const void *s1, const CVirtPtr<T, A> s2, TVirtPtrSize n)
{
    return private_utils::rawCompare(static_cast<const char *>(s1),
                                     static_cast<CVirtPtr<const char, A> >(s2), n, private_utils::memComparator, false);
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
{ return private_utils::rawCompare(dest, src, n, private_utils::strnComparator, true);}
template <typename A> int strncmp(CVirtPtr<const char, A> dest, const char *src, TVirtPtrSize n)
{ return private_utils::rawCompare(dest, src, n, private_utils::strnComparator, true); }
template <typename A> int strncmp(const char *dest, CVirtPtr<const char, A> src, TVirtPtrSize n)
{ return private_utils::rawCompare(dest, src, n, private_utils::strnComparator, true); }

template <typename A1, typename A2> int strcmp(CVirtPtr<const char, A1> dest, CVirtPtr<const char, A2> src)
{ return private_utils::rawCompare(dest, src, (TVirtPtrSize)-1, private_utils::strComparator, true); }
template <typename A> int strcmp(const char *dest, CVirtPtr<const char, A> src)
{ return private_utils::rawCompare(dest, src, (TVirtPtrSize)-1, private_utils::strComparator, true); }
template <typename A> int strcmp(CVirtPtr<const char, A> dest, const char *src)
{ return private_utils::rawCompare(dest, src, (TVirtPtrSize)-1, private_utils::strComparator, true); }

template <typename A> int strlen(CVirtPtr<const char, A> str)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (str.isWrapped())
        return ::strlen(str.unwrap());
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

}

#pragma pop_macro("min")
#pragma pop_macro("max")

#endif // VIRTMEM_UTILS_HPP

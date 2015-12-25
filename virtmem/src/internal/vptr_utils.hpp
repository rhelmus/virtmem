#ifndef VIRTMEM_UTILS_HPP
#define VIRTMEM_UTILS_HPP

/**
  @file
  @brief This header file contains several overloads of common C functions that work with virtual pointers.
  */

#include "config/config.h"
#include "utils.h"
#include "vptr.h"

#include <stdlib.h>
#include <string.h>

namespace virtmem {

namespace private_utils {

template <typename T, typename T2> struct TSameType { static const bool flag = false; };
template <typename T> struct TSameType<T, T> { static const bool flag = true; };

template <typename T> struct TVirtPtrTraits { };

template <typename T, typename A> struct TVirtPtrTraits<VPtr<T, A> >
{
    typedef VPtrLock<VPtr<T, A> > Lock;

    static bool isWrapped(VPtr<T, A> p) { return p.isWrapped(); }
    static T *unwrap(VPtr<T, A> p) { return p.unwrap(); }
    static bool isVirtPtr(void) { return true; }
    static Lock makeLock(VPtr<T, A> w, VirtPageSize s, bool ro=false)
    { return makeVirtPtrLock(w, s, ro); }
    static VirtPageSize getLockSize(Lock &l) { return l.getLockSize(); }
    static VirtPageSize getPageSize(void)
    { return A::getInstance()->getBigPageSize(); }

};

template <typename T> struct TVirtPtrTraits<T *>
{
    typedef T** Lock;

    static bool isWrapped(T *) { return false; }
    static T *unwrap(T *p) { return p; }
    static bool isVirtPtr(void) { return false; }
    static Lock makeLock(T *&p, VirtPageSize, __attribute__ ((unused)) bool ro=false) { return &p; }
    static VirtPageSize getLockSize(Lock &) { return (VirtPageSize)-1; }
    static VirtPageSize getPageSize(void) { return (VirtPageSize)-1; }
};

template <typename T1, typename T2, typename A> int ptrDiff(VPtr<T1, A> p1, VPtr<T2, A> p2) { return p2 - p1; }
template <typename T1, typename T2> int ptrDiff(T1 *p1, T2 *p2) { return p2 - p1; }
template <typename T1, typename T2> int ptrDiff(T1, T2) { return 0; } // dummy, not used (for mix of regular and virt pointers)
template <typename T1, typename T2, typename A> bool ptrEqual(VPtr<T1, A> p1, VPtr<T2, A> p2) { return p1 == p2; }
template <typename T1, typename T2> bool ptrEqual(T1 *p1, T2 *p2) { return p1 == p2; }
template <typename T1, typename T2> bool ptrEqual(T1, T2) { return false; } // mix of virt and regular pointers

template <typename T1, typename T2> VirtPageSize getMaxLockSize(T1 p1, T2 p2)
{
    VirtPageSize ret = minimal(TVirtPtrTraits<T1>::getPageSize(), TVirtPtrTraits<T2>::getPageSize());

    // check for overlap in case both p1 and p2 are virtual pointers from the same allocator
    if (TSameType<T1, T2>::flag && TVirtPtrTraits<T1>::isVirtPtr())
        ret = minimal(ret, static_cast<VirtPageSize>(abs(ptrDiff(p1, p2))));

    return ret;
}


// copier function that works with raw pointers for rawCopy, returns false if copying should be aborted
typedef bool (*RawCopier)(char *, const char *, VPtrSize);

// Generalized copy for memcpy and strncpy
template <typename T1, typename T2> T1 rawCopy(T1 dest, T2 src, VPtrSize size,
                                               RawCopier copier, bool checknull)
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

    VPtrSize sizeleft = size;
    const VirtPageSize maxlocksize = getMaxLockSize(dest, src);
    T1 p1 = dest;
    T2 p2 = src;

    while (sizeleft)
    {
        VirtPageSize cpsize = minimal(static_cast<VPtrSize>(maxlocksize), sizeleft);

        typename TVirtPtrTraits<T1>::Lock l1 = TVirtPtrTraits<T1>::makeLock(p1, cpsize);
        cpsize = minimal(cpsize, TVirtPtrTraits<T1>::getLockSize(l1));
        typename TVirtPtrTraits<T2>::Lock l2 = TVirtPtrTraits<T2>::makeLock(p2, cpsize, true);
        cpsize = minimal(cpsize, TVirtPtrTraits<T2>::getLockSize(l2));

        if (!copier(*l1, *l2, cpsize))
            return dest;

        p1 += cpsize; p2 += cpsize;
        sizeleft -= cpsize;
        ASSERT(sizeleft <= size);
    }

    return dest;
}

// comparison function for rawCompare
typedef int (*RawComparator)(const char *, const char *, VPtrSize, bool &);

// Generalized compare for memcmp/strncmp
template <typename T1, typename T2> int rawCompare(T1 p1, T2 p2, VPtrSize n, RawComparator comparator,
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

    VPtrSize sizeleft = n;
    const VirtPageSize maxlocksize = getMaxLockSize(p1, p2);

    while (sizeleft)
    {
        VirtPageSize cmpsize = minimal(static_cast<VPtrSize>(maxlocksize), sizeleft);
        typename TVirtPtrTraits<T1>::Lock l1 = TVirtPtrTraits<T1>::makeLock(p1, cmpsize, true);
        cmpsize = minimal(cmpsize, TVirtPtrTraits<T1>::getLockSize(l1));
        typename TVirtPtrTraits<T2>::Lock l2 = TVirtPtrTraits<T2>::makeLock(p2, cmpsize, true);
        cmpsize = minimal(cmpsize, TVirtPtrTraits<T2>::getLockSize(l2));

        int cmp = comparator(*l1, *l2, cmpsize, done);
        if (cmp != 0 || done)
            return cmp;

        p1 += cmpsize; p2 += cmpsize;
        sizeleft -= cmpsize;
        ASSERT(sizeleft <= n);
    }

    return 0;
}

inline bool memCopier(char *dest, const char *src, VPtrSize n)
{
    memcpy(dest, src, n);
    return true;
}

inline bool strnCopier(char *dest, const char *src, VPtrSize n)
{
    return n && (strncpy(dest, src, n))[n-1] != 0; // strncpy pads with zero
}

inline bool strCopier(char *dest, const char *src, VPtrSize n)
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

inline int memComparator(const char *p1, const char *p2, VPtrSize n, bool &)
{ return ::memcmp(p1, p2, n); }
inline int strnComparator(const char *p1, const char *p2, VPtrSize n, bool &)
{ return ::strncmp(p1, p2, n); }
inline int strComparator(const char *p1, const char *p2, VPtrSize n, bool &done)
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


// Specialize memcpy/memset/memcmp for char types: we may have to convert to this type anyway,
// and this saves some template code duplication for other types. A general template function will just
// cast and call the specialized function.


/**
 * @anchor Coverloads
 * @name Overloads of C library functions for virtual pointers
 * The following functions are overloads of some common C functions for dealing with memory and strings.
 * They accept virtual pointers or a mix of virtual and regular pointers. Please note that they are
 * defined in the [virtmem namespace](@ref virtmem) like any other code from `virtmem`, hence, they will not
 * "polute" the global namespace unless you want to (i.e. by using the `using` directive).
 * @{
 **/

template <typename T1, typename A1, typename T2, typename A2>
VPtr<T1, A1> memcpy(VPtr<T1, A1> dest, const VPtr<T2, A2> src, VPtrSize size)
{
    return static_cast<VPtr<T1, A1> >(
                private_utils::rawCopy(static_cast<VPtr<char, A1> >(dest),
                                       static_cast<const VPtr<const char, A2> >(src), size,
                                       private_utils::memCopier, false));
}

template <typename T, typename A> VPtr<T, A> memcpy(VPtr<T, A> dest, const void *src, VPtrSize size)
{
    return static_cast<VPtr<T, A> >(
                private_utils::rawCopy(static_cast<VPtr<char, A> >(dest),
                                       static_cast<const char *>(src), size,
                                       private_utils::memCopier, false));
}

template <typename T, typename A> void *memcpy(void *dest, VPtr<T, A> src, VPtrSize size)
{
    return private_utils::rawCopy(static_cast<char *>(dest),
                                  static_cast<const VPtr<const char, A> >(src), size,
                                  private_utils::memCopier, false);
}

template <typename A> VPtr<char, A> memset(VPtr<char, A> dest, int c, VPtrSize size)
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

    VPtrSize sizeleft = size;
    VPtr<char, A> p = dest;

    while (sizeleft)
    {
        VirtPageSize setsize = private_utils::minimal((VPtrSize)A::getInstance()->getBigPageSize(), sizeleft);
        VPtrLock<VPtr<char, A> > l = makeVirtPtrLock(p, setsize);
        setsize = l.getLockSize();
        ::memset(*l, c, setsize);
        p += setsize; sizeleft -= setsize;
    }

    return dest;
}

template <typename T, typename A> VPtr<T, A> memset(VPtr<T, A> dest, int c, VPtrSize size)
{
    return static_cast<VPtr<T, A> >(memset(static_cast<VPtr<char, A> >(dest), c, size));
}

template <typename T1, typename A1, typename T2, typename A2> int memcmp(VPtr<T1, A1> s1, const VPtr<T2, A2> s2,
                                                                         VPtrSize n)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (s1.isWrapped() && s2.isWrapped())
        return ::memcmp(s1.unwrap(), s2.unwrap(), n);
#endif

    return private_utils::rawCompare(static_cast<VPtr<const char, A1> >(s1),
                                     static_cast<VPtr<const char, A2> >(s2), n, private_utils::memComparator, false);
}

template <typename T, typename A> int memcmp(VPtr<T, A> s1, const void *s2, VPtrSize n)
{
    return private_utils::rawCompare(static_cast<VPtr<const char, A> >(s1),
                                     static_cast<const char *>(s2), n, private_utils::memComparator, false);
}

template <typename T, typename A> int memcmp(const void *s1, const VPtr<T, A> s2, VPtrSize n)
{
    return private_utils::rawCompare(static_cast<const char *>(s1),
                                     static_cast<VPtr<const char, A> >(s2), n, private_utils::memComparator, false);
}

template <typename A1, typename A2> VPtr<char, A1> strncpy(VPtr<char, A1> dest, const VPtr<const char, A2> src,
                                                              VPtrSize n)
{
    return private_utils::rawCopy(dest, src, n, private_utils::strnCopier, true);
}

template <typename A> VPtr<char, A> strncpy(VPtr<char, A> dest, const char *src, VPtrSize n)
{
    return private_utils::rawCopy(dest, src, n, private_utils::strnCopier, true);
}

template <typename A> char *strncpy(char *dest, const VPtr<const char, A> src, VPtrSize n)
{
    return private_utils::rawCopy(dest, src, n, private_utils::strnCopier, true);
}

template <typename A1, typename A2> VPtr<char, A1> strcpy(VPtr<char, A1> dest, const VPtr<const char, A2> src)
{
    return private_utils::rawCopy(dest, src, (VPtrSize)-1, private_utils::strCopier, true);
}

template <typename A> VPtr<char, A> strcpy(VPtr<char, A> dest, const char *src)
{
    return private_utils::rawCopy(dest, src, (VPtrSize)-1, private_utils::strCopier, true);
}

template <typename A> char *strcpy(char *dest, const VPtr<const char, A> src)
{
    return private_utils::rawCopy(dest, src, (VPtrSize)-1, private_utils::strCopier, true);
}

template <typename A1, typename A2> int strncmp(VPtr<const char, A1> dest, VPtr<const char, A2> src, VPtrSize n)
{ return private_utils::rawCompare(dest, src, n, private_utils::strnComparator, true);}
template <typename A> int strncmp(VPtr<const char, A> dest, const char *src, VPtrSize n)
{ return private_utils::rawCompare(dest, src, n, private_utils::strnComparator, true); }
template <typename A> int strncmp(const char *dest, VPtr<const char, A> src, VPtrSize n)
{ return private_utils::rawCompare(dest, src, n, private_utils::strnComparator, true); }

template <typename A1, typename A2> int strcmp(VPtr<const char, A1> dest, VPtr<const char, A2> src)
{ return private_utils::rawCompare(dest, src, (VPtrSize)-1, private_utils::strComparator, true); }
template <typename A> int strcmp(const char *dest, VPtr<const char, A> src)
{ return private_utils::rawCompare(dest, src, (VPtrSize)-1, private_utils::strComparator, true); }
template <typename A> int strcmp(VPtr<const char, A> dest, const char *src)
{ return private_utils::rawCompare(dest, src, (VPtrSize)-1, private_utils::strComparator, true); }

template <typename A> int strlen(VPtr<const char, A> str)
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

template <typename A1, typename A2> VPtr<char, A1> strncpy(VPtr<char, A1> dest, const VPtr<char, A2> src,
                                                              VPtrSize n)
{ return strncpy(dest, static_cast<const VPtr<const char, A2> >(src), n); }
template <typename A> char *strncpy(char *dest, const VPtr<char, A> src, VPtrSize n)
{ return strncpy(dest, static_cast<const VPtr<const char, A> >(src), n); }

template <typename A1, typename A2> VPtr<char, A1> strcpy(VPtr<char, A1> dest, const VPtr<char, A2> src)
{ return strcpy(dest, static_cast<const VPtr<const char, A2> >(src)); }
template <typename A> char *strcpy(char *dest, const VPtr<char, A> src)
{ return strcpy(dest, static_cast<const VPtr<const char, A> >(src)); }

template <typename A1, typename A2> int strncmp(VPtr<char, A1> dest, VPtr<char, A2> src, VPtrSize n)
{ return strncmp(static_cast<VPtr<const char, A1> >(dest), static_cast<VPtr<const char, A2> >(src), n); }
template <typename A1, typename A2> int strncmp(VPtr<const char, A1> dest, VPtr<char, A2> src, VPtrSize n)
{ return strncmp(dest, static_cast<VPtr<const char, A2> >(src), n); }
template <typename A1, typename A2> int strncmp(VPtr<char, A1> dest, VPtr<const char, A2> src, VPtrSize n)
{ return strncmp(static_cast<VPtr<const char, A1> >(dest), src, n); }
template <typename A> int strncmp(const char *dest, VPtr<char, A> src, VPtrSize n)
{ return strncmp(dest, static_cast<VPtr<const char, A> >(src), n); }
template <typename A> int strncmp(VPtr<char, A> dest, const char *src, VPtrSize n)
{ return strncmp(static_cast<VPtr<const char, A> >(dest), src, n); }

template <typename A1, typename A2> int strcmp(VPtr<char, A1> dest, VPtr<char, A2> src)
{ return strcmp(static_cast<VPtr<const char, A1> >(dest), static_cast<VPtr<const char, A2> >(src)); }
template <typename A1, typename A2> int strcmp(VPtr<const char, A1> dest, VPtr<char, A2> src)
{ return strcmp(dest, static_cast<VPtr<const char, A2> >(src)); }
template <typename A1, typename A2> int strcmp(VPtr<char, A1> dest, VPtr<const char, A2> src)
{ return strcmp(static_cast<VPtr<const char, A1> >(dest), src); }
template <typename A> int strcmp(const char *dest, VPtr<char, A> src)
{ return strcmp(dest, static_cast<VPtr<const char, A> >(src)); }
template <typename A> int strcmp(VPtr<char, A> dest, const char *src)
{ return strcmp(static_cast<VPtr<const char, A> >(dest), src); }

template <typename A> int strlen(VPtr<char, A> str) { return strlen(static_cast<VPtr<const char, A> >(str)); }

// @}

}

#endif // VIRTMEM_UTILS_HPP

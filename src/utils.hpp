#ifndef UTILS_HPP
#define UTILS_HPP

#include "config.h"

// Specialize memcpy/memset for uint8_t types: we have to convert to this type anyway, and this saves some
// template code duplication for other types. A general template function will just
// cast to the specialized function.

template <typename A> CVirtPtr<uint8_t, A> memcpy(CVirtPtr<uint8_t, A> dest, const CVirtPtr<uint8_t, A> src,
                                                  TVirtPtrSize size)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (dest.isWrapped() && src.isWrapped())
    {
        memcpy(dest.unwrap(), src.unwrap(), size);
        return dest;
    }
    else if (dest.isWrapped())
    {
        memcpy(dest.unwrap(), src, size);
        return dest;
    }
    else if (src.isWrapped())
        return memcpy(dest, src.unwrap(), size);
#endif

    // If dest and src can be locked, use regular memcpy
    if (A::getInstance()->unlockedPages() >= 2)
    {
        TVirtPtrSize start = 0;
        const TVirtPtrSize psize = A::getInstance()->getPageSize();
        while (start < size)
        {
            memcpy(*makePtrWrapLock(dest + start), *makePtrWrapLock(src + start, true), psize);
            start += psize;
        }
    }
    else
    {
        for (TVirtPtrSize s=0; s<size; ++s)
            ((CVirtPtr<uint8_t, A>)dest)[s] = ((CVirtPtr<uint8_t, A>)src)[s];
    }

    return dest;
}

template <typename T, typename A> CVirtPtr<T, A> memcpy(CVirtPtr<T, A> dest, const CVirtPtr<T, A> src,
                                                        TVirtPtrSize size)
{
    return memcpy(static_cast<CVirtPtr<uint8_t, A> >(dest), static_cast<const CVirtPtr<uint8_t, A> >(src), size);
}

template <typename A> CVirtPtr<uint8_t, A> memcpy(CVirtPtr<uint8_t, A> dest, const void *src,
                                                  TVirtPtrSize size)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (dest.isWrapped())
    {
        memcpy(dest.unwrap(), src, size);
        return dest;
    }
#endif

    // If dest can be locked, use regular memcpy
    if (A::getInstance()->unlockedPages() >= 1)
    {
        TVirtPtrSize start = 0;
        const TVirtPtrSize psize = A::getInstance()->getPageSize();

        while (start < size)
        {
            memcpy(*makePtrWrapLock(dest + start), (uint8_t *)src + start, psize);
            start += psize;
        }
    }
    else
    {
        for (TVirtPtrSize s=0; s<size; ++s)
            dest[s] = ((uint8_t *)src)[s];
    }
    return dest;
}

template <typename T, typename A> CVirtPtr<T, A> memcpy(CVirtPtr<T, A> dest, const void *src, TVirtPtrSize size)
{
    return memcpy(static_cast<CVirtPtr<uint8_t, A> >(dest), src, size);
}

template <typename A> void *memcpy(void *dest, const CVirtPtr<uint8_t, A> src, TVirtPtrSize size)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (src.isWrapped())
        return memcpy(dest, src.unwrap(), size);
#endif

    // If src can be locked, use regular memcpy
    if (A::getInstance()->unlockedPages() >= 1)
    {
        TVirtPtrSize start = 0;
        const TVirtPtrSize psize = A::getInstance()->getPageSize();

        while (start < size)
        {
            memcpy((uint8_t *)dest + start, *makePtrWrapLock(src + start, true), psize);
            start += psize;
        }
    }
    else
    {
        for (TVirtPtrSize s=0; s<size; ++s)
            ((uint8_t *)dest)[s] = src[s];
    }
    return dest;
}

template <typename T, typename A> void *memcpy(void *dest, const CVirtPtr<T, A> src, TVirtPtrSize size)
{
    return memcpy(dest, static_cast<const CVirtPtr<uint8_t, A> >(src), size);
}

template <typename A> CVirtPtr<uint8_t, A> memset(CVirtPtr<uint8_t, A> dest, int c, TVirtPtrSize size)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (dest.isWrapped())
    {
        memset(dest.unwrap(), c, size);
        return dest;
    }
#endif

    // If dest can be locked, use regular memset
    if (A::getInstance()->unlockedPages() >= 1)
    {
        TVirtPtrSize start = 0;
        const TVirtPtrSize psize = A::getInstance()->getPageSize();
        while (start < size)
        {
            memset(*makePtrWrapLock(dest + start), c, psize);
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
    return memset(static_cast<CVirtPtr<uint8_t, A> >(dest), c, size);
}

template <typename A> CVirtPtr<char, A> strncpy(CVirtPtr<char, A> dest, const CVirtPtr<char, A> src,
                                                TVirtPtrSize n)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (dest.isWrapped() && src.isWrapped())
    {
        strncpy(dest.unwrap(), src.unwrap(), n);
        return dest;
    }
    else if (dest.isWrapped())
    {
        strncpy(dest.unwrap(), src, n);
        return dest;
    }
    else if (src.isWrapped())
        return strncpy(dest, src.unwrap(), n);
#endif

    // If dest and src can be locked, use regular strncpy
    if (A::getInstance()->unlockedPages() >= 2)
    {
        TVirtPtrSize start = 0;
        const TVirtPtrSize psize = A::getInstance()->getPageSize();
        while (true)
        {
            strncpy(*makePtrWrapLock(dest + start), *makePtrWrapLock(src + start, true), psize);
            start += psize;
            if (start >= n || dest[start - 1] == 0) // finished?
                break;
        }
    }
    else
    {
        for (TVirtPtrSize s=0; s<n; ++s)
        {
            if ((dest[s] = src[s]) == 0)
                break;
        }
    }

    return dest;
}

template <typename A> CVirtPtr<char, A> strncpy(CVirtPtr<char, A> dest, const char *src, TVirtPtrSize n)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (dest.isWrapped())
    {
        strncpy(dest.unwrap(), src, n);
        return dest;
    }
#endif

    // If dest can be locked, use regular strncpy
    if (A::getInstance()->unlockedPages() >= 1)
    {
        TVirtPtrSize start = 0;
        const TVirtPtrSize psize = A::getInstance()->getPageSize();
        while (true)
        {
            strncpy(*makePtrWrapLock(dest + start), src + start, psize);
            start += psize;
            if (start >= n || dest[start - 1] == 0) // finished?
                break;
        }
    }
    else
    {
        for (TVirtPtrSize s=0; s<n; ++s)
        {
            if ((dest[s] = src[s]) == 0)
                break;
        }
    }

    return dest;
}

template <typename A> CVirtPtr<char, A> strncpy(char *dest, const CVirtPtr<char, A> src, TVirtPtrSize n)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (src.isWrapped())
        return strncpy(dest, src.unwrap(), n);
#endif

    // If src can be locked, use regular strncpy
    if (A::getInstance()->unlockedPages() >= 1)
    {
        TVirtPtrSize start = 0;
        const TVirtPtrSize psize = A::getInstance()->getPageSize();
        while (true)
        {
            strncpy(dest + start, *makePtrWrapLock(src + start), psize);
            start += psize;
            if (start >= n || dest[start - 1] == 0) // finished?
                break;
        }
    }
    else
    {
        for (TVirtPtrSize s=0; s<n; ++s)
        {
            if ((dest[s] = src[s]) == 0)
                break;
        }
    }

    return dest;
}

template <typename A> CVirtPtr<char, A> strcpy(CVirtPtr<char, A> dest, const CVirtPtr<char, A> src)
{
    return strncpy(dest, src, (TVirtPtrSize)-1);
}

template <typename A> CVirtPtr<char, A> strcpy(CVirtPtr<char, A> dest, const char *src)
{
    return strncpy(dest, src, (TVirtPtrSize)-1);
}

template <typename A> char *strcpy(char *dest, const CVirtPtr<char, A> src)
{
    return strncpy(dest, src, (TVirtPtrSize)-1);
}

// Generic version
template <typename T1, typename T2> int strncmp_G(T1 dest, T2 src, TVirtPtrSize n)
{
    for (TVirtPtrSize i=0; i<n; ++i, ++dest, ++src)
    {
        if (*dest < *src)
            return -1;
        if (*dest > *src)
            return 1;
        if (*dest == 0 /*|| *src == 0*/) // if we are here, both must be (non)zero
            break;
    }

    return 0;
}

template <typename A> int strncmp(CVirtPtr<char, A> dest, CVirtPtr<char, A> src, TVirtPtrSize n)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (dest.isWrapped() && src.isWrapped())
    {
        return strncmp(dest.unwrap(), src.unwrap(), n);
        return dest;
    }
#endif
    return strncmp_G(dest, src, n);
}

template <typename A> int strncmp(CVirtPtr<char, A> dest, const char *src, TVirtPtrSize n)
{ return strncmp_G(dest, src, n); }
template <typename A> int strncmp(const char *dest, CVirtPtr<char, A> src, TVirtPtrSize n)
{ return strncmp_G(dest, src, n); }

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

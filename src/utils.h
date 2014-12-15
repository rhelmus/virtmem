#ifndef UTILS_H
#define UTILS_H

#include "wrapper.h"

#include <string.h>
#include <stdio.h> // UNDONE

// Generic NULL type
class CNILL
{
public:
    template <typename T> inline operator T*(void) const { return 0; }
    template <typename T, typename A> inline operator CVirtPtr<T, A>(void) const { return CVirtPtr<T, A>(); }
    inline operator CVirtPtrBase(void) const { return CVirtPtrBase(); }
    template <typename T, typename A> inline operator typename CVirtPtr<T, A>::CValueWrapper(void) const { return CVirtPtr<T, A>::CValueWrapper(0); }

} extern const NILL;

template <typename TV> class CPtrWrapLock
{
    static int locks;
    TV ptrWrap;
    bool readOnly;

public:
    CPtrWrapLock(const TV &w, bool ro=false) : ptrWrap(w) { lock(ro); }
    ~CPtrWrapLock(void) { unlock(); }

    void lock(bool ro=false)
    {
        ++locks;
        if (!ptrWrap.isWrapped())
            TV::getAlloc()->lock(ptrWrap.ptr, ro);
        readOnly = ro;
        printf("locks: %d\n", locks);
    }
    void unlock(void)
    {
        --locks;
        if (!ptrWrap.isWrapped())
            TV::getAlloc()->unlock(ptrWrap.ptr);
        printf("locks: %d\n", locks);
    }
    typename TV::TPtr operator *(void) { return ptrWrap.read(readOnly); }
};

template <typename T> int CPtrWrapLock<T>::locks;

// Shortcut
template <typename T> CPtrWrapLock<T> makePtrWrapLock(const T &w, bool ro=false) { return CPtrWrapLock<T>(w, ro); }

template <typename T, typename A> CVirtPtr<T, A> memcpy(CVirtPtr<T, A> &dest, const CVirtPtr<T, A> &src,
                                                        TVirtSizeType size)
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
        TVirtSizeType start = 0;
        const TVirtSizeType psize = A::getInstance()->getPageSize();
        while (start < size)
        {
            memcpy(*makePtrWrapLock(dest + start), *makePtrWrapLock(src + start, true), psize);
            start += psize;
        }
    }
    else
    {
        for (size_t s=0; s<size; ++s)
            ((CVirtPtr<uint8_t, A>)dest)[s] = ((CVirtPtr<uint8_t, A>)src)[s];
    }
    return dest;
}

template <typename T, typename A> CVirtPtr<T, A> memcpy(CVirtPtr<T, A> &dest, const void *src, TVirtSizeType size)
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
        TVirtSizeType start = 0;
        const TVirtSizeType psize = A::getInstance()->getPageSize();

        while (start < size)
        {
            memcpy(*makePtrWrapLock(dest + start), (uint8_t *)src + start, psize);
            start += psize;
        }
    }
    else
    {
        for (size_t s=0; s<size; ++s)
            ((CVirtPtr<uint8_t, A>)dest)[s] = ((uint8_t *)src)[s];
    }
    return dest;
}

template <typename T, typename A> void *memcpy(void *dest, const CVirtPtr<T, A> &src, TVirtSizeType size)
{
#ifdef VIRTMEM_WRAP_CPOINTERS
    if (src.isWrapped())
        return memcpy(dest, src.unwrap(), size);
#endif

    // If src can be locked, use regular memcpy
    if (A::getInstance()->unlockedPages() >= 1)
    {
        TVirtSizeType start = 0;
        const TVirtSizeType psize = A::getInstance()->getPageSize();

        while (start < size)
        {
            memcpy((uint8_t *)dest + start, *makePtrWrapLock(src + start, true), psize);
            start += psize;
        }
    }
    else
    {
        for (size_t s=0; s<size; ++s)
            ((uint8_t *)dest)[s] = ((CVirtPtr<uint8_t, A>)src)[s];
    }
    return dest;
}

template <typename T, typename A> CVirtPtr<T, A> memset(CVirtPtr<T, A> dest, int c, TVirtSizeType size)
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
        TVirtSizeType start = 0;
        const TVirtSizeType psize = A::getInstance()->getPageSize();
        while (start < size)
        {
            memset(*makePtrWrapLock(dest + start), c, psize);
            start += psize;
        }
    }
    else
    {
        for (size_t s=0; s<size; ++s)
            ((CVirtPtr<uint8_t, A>)dest)[s] = c;
    }
    return dest;
}

template <typename A> CVirtPtr<char, A> strncpy(CVirtPtr<char, A> &dest, const CVirtPtr<char, A> &src,
                                                TVirtSizeType n)
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
        TVirtSizeType start = 0;
        const TVirtSizeType psize = A::getInstance()->getPageSize();
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
        for (size_t s=0; s<n; ++s)
        {
            if ((dest[s] = src[s]) == 0)
                break;
        }
    }

    return dest;
}

template <typename A> CVirtPtr<char, A> strncpy(CVirtPtr<char, A> &dest, const char *src,
                                                TVirtSizeType n)
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
        TVirtSizeType start = 0;
        const TVirtSizeType psize = A::getInstance()->getPageSize();
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
        for (size_t s=0; s<n; ++s)
        {
            if ((dest[s] = src[s]) == 0)
                break;
        }
    }

    return dest;
}

#endif // UTILS_H

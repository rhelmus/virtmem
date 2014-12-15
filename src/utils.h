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

#include "utils.hpp"

#endif // UTILS_H

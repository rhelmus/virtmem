#ifndef UTILS_H
#define UTILS_H

#include "wrapper.h"

// Generic NULL type
class CNILL
{
public:
    template <typename T> inline operator T*(void) const { return 0; }
    template <typename T, typename A> inline operator CVirtPtr<T, A>(void) const { return CVirtPtr<T, A>(); }
    inline operator CVirtPtrBase(void) const { return CVirtPtrBase(); }
    template <typename T, typename A> inline operator typename CVirtPtr<T, A>::CValueWrapper(void) const { return CVirtPtr<T, A>::CValueWrapper(0); }

} extern const NILL;

class CPtrWrapLock
{
    static int locks;
    CVirtPtrBase ptrWrap;

public:
    CPtrWrapLock(const CVirtPtrBase &w) : ptrWrap(w) { ++locks; /*printf("locks: %d\n", locks);*/ }
    ~CPtrWrapLock(void) { unlock(); }

    void unlock(void) { --locks; /*printf("locks: %d\n", locks);*/ }
//    void *operator &(void) { return ptrWrap.ptr; }
};

#endif // UTILS_H

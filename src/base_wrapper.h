#ifndef BASE_WRAPPER_H
#define BASE_WRAPPER_H

#include "alloc.h"

#if __cplusplus > 199711L
#define EXPLICIT explicit
#else
#define EXPLICIT
#endif

template <typename, typename> class CVirtPtr;

class CVirtPtrBase
{
    struct SNull { };

    // Safe bool idiom from boost::function
    struct SDummy { void nonNull(void) { } };
    typedef void (SDummy::*TSafeBool)(void);

    template <typename, typename> friend class CVirtPtr;
    template <typename> friend class CPtrWrapLock;

    enum { WRAP_BIT = sizeof(TVirtPointer) * 8 - 1 }; // Last bit is used to check if wrapping real pointer

protected:
    TVirtPointer ptr;

    // Return 'real' address of pointer, ie without wrapping bit
    // static so that CValueWrapper can use it as well
    static TVirtPointer getPtrAddr(TVirtPointer p) { return p & ~(1 << WRAP_BIT); }
    TVirtPointer getPtrAddr(void) const { return getPtrAddr(ptr); } // Shortcut

    static bool isWrapped(TVirtPointer p) { return p & (1<<WRAP_BIT); }
    bool isWrapped(void) const { return isWrapped(ptr); }

    static TVirtPointer getWrapped(TVirtPointer p) { return p | (1 << WRAP_BIT); }

public:
    // HACK: this allows constructing CVirtPtr objects from CVirtPtrBase variables, similar to
    // initializing non void pointers with a void pointer
    // Note that we could have used a copy constructor in CVirtPtr instead, but this would make the latter
    // class non-POD
    template <typename T, typename A> EXPLICIT operator CVirtPtr<T, A>(void) const { CVirtPtr<T, A> ret; ret.ptr = ptr; return ret; }

    // allow checking with NULL
    inline bool operator==(const SNull *) const { return getPtrAddr() == 0; }
    friend inline bool operator==(const SNull *, const CVirtPtrBase &pw) { return pw.getPtrAddr() == 0; }
    inline bool operator!=(const SNull *) const { return getPtrAddr() != 0; }
    inline operator TSafeBool (void) const { return getPtrAddr() == 0 ? 0 : &SDummy::nonNull; }

    inline bool operator==(const CVirtPtrBase &pb) const { return ptr == pb.ptr; }
    inline bool operator!=(const CVirtPtrBase &pb) const { return ptr != pb.ptr; }
    inline bool operator<(const CVirtPtrBase &pb) const { return ptr < pb.ptr; }
    inline bool operator<=(const CVirtPtrBase &pb) const { return ptr <= pb.ptr; }
    inline bool operator>=(const CVirtPtrBase &pb) const { return ptr >= pb.ptr; }
    inline bool operator>(const CVirtPtrBase &pb) const { return ptr > pb.ptr; }
};

#endif // BASE_WRAPPER_H

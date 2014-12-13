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

    template<typename, typename> friend class CVirtPtr;
    template<typename, bool> friend class CAllocProxy;
    friend class CPtrWrapLock;

protected:
    TVirtPointer ptr;

public:
//    template <typename T2> static inline CVirtPtr<T2> ptrWrap(const T2 *p) { CVirtPtr<T2> ret; ret.ptr = (void *)p; return ret; }
    //static inline CVirtPtrBase ptrWrap(void *p) { CVirtPtrBase ret; ret.ptr = (void *)p; return ret; }
//    template<typename T2> static inline T2 *ptrUnwrap(const CVirtPtr<T2> &p) { return static_cast<T2 *>(p.ptr); } // UNDONE
//    static inline void *ptrUnwrap(const CVirtPtrBase &p) { return p.ptr; } // UNDONE

    // HACK: this allows constructing CVirtPtr objects from CVirtPtrBase variables, similar to
    // initializing non void pointers with a void pointer
    // Note that we could have used a copy constructor in CVirtPtr instead, but this would make the latter
    // class non-POD
    template <typename T, typename A> EXPLICIT operator CVirtPtr<T, A>(void) const { CVirtPtr<T, A> ret; ret.ptr = ptr; return ret; }

    // allow checking with NULL
    inline bool operator==(const SNull *) const { return ptr == 0; }
    friend inline bool operator==(const SNull *, const CVirtPtrBase &pw) { return pw.ptr == 0; }
    inline bool operator!=(const SNull *) const { return ptr != 0; }
    inline operator TSafeBool (void) const { return ptr == 0 ? 0 : &SDummy::nonNull; }

    inline bool operator==(const CVirtPtrBase &pb) const { return ptr == pb.ptr; }
    inline bool operator!=(const CVirtPtrBase &pb) const { return ptr != pb.ptr; }
    inline bool operator<(const CVirtPtrBase &pb) const { return ptr < pb.ptr; }
    inline bool operator<=(const CVirtPtrBase &pb) const { return ptr <= pb.ptr; }
    inline bool operator>=(const CVirtPtrBase &pb) const { return ptr >= pb.ptr; }
    inline bool operator>(const CVirtPtrBase &pb) const { return ptr > pb.ptr; }
};

#endif // BASE_WRAPPER_H

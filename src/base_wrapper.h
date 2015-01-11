#ifndef BASE_WRAPPER_H
#define BASE_WRAPPER_H

#include "config.h"
#include "alloc.h"
#include "utils.h"

#if __cplusplus > 199711L
#define EXPLICIT explicit
#else
#define EXPLICIT
#endif

class CNILL;

template <bool, typename T1, typename T2> struct TConditional
{
    typedef T1 type;
};

template <typename T1, typename T2> struct TConditional<false, T1, T2>
{
    typedef T2 type;
};

template <typename, typename> class CVirtPtr;

class CVirtPtrBase
{
    struct SNull { };

    // Safe bool idiom from boost::function
    struct SDummy { void nonNull(void) { } };
    typedef void (SDummy::*TSafeBool)(void);

    template <typename, typename> friend class CVirtPtr;
    template <typename> friend class CPtrWrapLock;

public:
#ifdef VIRTMEM_WRAP_CPOINTERS
#if defined(__x86_64__) || defined(_M_X64)
    typedef __uint128_t TPtrNum;
#elif defined(__i386) || defined(_M_IX86)
    typedef uint64_t TPtrNum;
#else
    typedef TConditional<(sizeof(intptr_t) > sizeof(TVirtPointer)), intptr_t, TVirtPointer>::type TPtrNum;
#endif
#else
    typedef TVirtPointer TPtrNum;
#endif

private:
    enum { WRAP_BIT = sizeof(TPtrNum) * 8 - 1 }; // Last bit is used to check if wrapping real pointer

protected:
    // UNDONE
    // For debug
#if defined(__x86_64__) || defined(_M_X64)
    union
    {
        TPtrNum ptr;
        struct
        {
            TVirtPointer addr, wrapped;
        } ugh;
    };
#else
    TPtrNum ptr;
#endif
    // Return 'real' address of pointer, ie without wrapping bit
    // static so that CValueWrapper can use it as well
    static intptr_t getPtrNum(TPtrNum p) { return p & ~((TPtrNum)1 << WRAP_BIT); }
    intptr_t getPtrNum(void) const { return getPtrNum(ptr); } // Shortcut

    static void *unwrap(TPtrNum p) { ASSERT(isWrapped(p)); return reinterpret_cast<void *>(getPtrNum(p)); }

public:
    static TPtrNum getWrapped(TPtrNum p) { return p | ((TPtrNum)1 << WRAP_BIT); }

    static bool isWrapped(TPtrNum p) { return p & ((TPtrNum)1 << WRAP_BIT); }
    bool isWrapped(void) const { return isWrapped(ptr); }

    TPtrNum getRawNum(void) const { return ptr; }
    void setRawNum(TPtrNum p) { ptr = p; }

    static CVirtPtrBase wrap(const void *p)
    {
        CVirtPtrBase ret;
        ret.ptr = getWrapped(reinterpret_cast<TPtrNum>(p));
        return ret;
    }
    void *unwrap(const CVirtPtrBase &p) {  return unwrap(p); }
    void *unwrap(void) { return unwrap(ptr); }
    const void *unwrap(void) const { return unwrap(ptr); }

    // HACK: this allows constructing CVirtPtr objects from CVirtPtrBase variables, similar to
    // initializing non void pointers with a void pointer
    // Note that we could have used a copy constructor in CVirtPtr instead, but this would make the latter
    // class non-POD
    template <typename T, typename A> EXPLICIT operator CVirtPtr<T, A>(void) const { CVirtPtr<T, A> ret; ret.ptr = ptr; return ret; }

    // allow checking with NULL
    inline bool operator==(const SNull *) const { return getPtrNum() == 0; }
    friend inline bool operator==(const SNull *, const CVirtPtrBase &pw) { return pw.getPtrNum() == 0; }
    inline bool operator==(const CNILL &) const { return getPtrNum() == 0; }
    friend inline bool operator==(const CNILL &, const CVirtPtrBase &pw) { return pw.getPtrNum() == 0; }
    inline bool operator!=(const SNull *) const { return getPtrNum() != 0; }
    friend inline bool operator!=(const SNull *, const CVirtPtrBase &pw) { return pw.getPtrNum() != 0; }
    inline bool operator!=(const CNILL &) const { return getPtrNum() != 0; }
    friend inline bool operator!=(const CNILL &, const CVirtPtrBase &pw) { return pw.getPtrNum() != 0; }
    inline operator TSafeBool (void) const { return getPtrNum() == 0 ? 0 : &SDummy::nonNull; }

    // UNDONE: _int128_t comparison sometimes fails??? Perhaps only check for ptrnums/iswrapped when on x86-64? Test this!
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    inline bool operator==(const CVirtPtrBase &pb) const { return getPtrNum() == pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
    inline bool operator!=(const CVirtPtrBase &pb) const { return getPtrNum() != pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
    inline bool operator<(const CVirtPtrBase &pb) const { return getPtrNum() < pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
    inline bool operator<=(const CVirtPtrBase &pb) const { return getPtrNum() <= pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
    inline bool operator>=(const CVirtPtrBase &pb) const { return getPtrNum() >= pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
    inline bool operator>(const CVirtPtrBase &pb) const { return getPtrNum() > pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
#else
    inline bool operator==(const CVirtPtrBase &pb) const { return ptr == pb.ptr; }
    inline bool operator!=(const CVirtPtrBase &pb) const { return ptr != pb.ptr; }
    inline bool operator<(const CVirtPtrBase &pb) const { return ptr < pb.ptr; }
    inline bool operator<=(const CVirtPtrBase &pb) const { return ptr <= pb.ptr; }
    inline bool operator>=(const CVirtPtrBase &pb) const { return ptr >= pb.ptr; }
    inline bool operator>(const CVirtPtrBase &pb) const { return ptr > pb.ptr; }
#endif

};

#endif // BASE_WRAPPER_H

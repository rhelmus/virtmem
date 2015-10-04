#ifndef VIRTMEM_BASE_WRAPPER_H
#define VIRTMEM_BASE_WRAPPER_H

/**
  * @file
  * @brief This file contains the virtual pointer base class
  */

#include "config.h"
#include "alloc.h"
#include "utils.h"

#if __cplusplus > 199711L
#define EXPLICIT explicit
#else
#define EXPLICIT
#endif

namespace virtmem {

class CNILL;

namespace private_utils {

template <bool, typename T1, typename T2> struct TConditional
{
    typedef T1 type;
};

template <typename T1, typename T2> struct TConditional<false, T1, T2>
{
    typedef T2 type;
};

}

template <typename, typename> class CVirtPtr;

/**
 * @brief This is the base class of CVirtPtr and can be used for typeless pointers.
 *
 * This base class contains all the functionality of virtual pointers which do not depend
 * on any templated code to reduce code size.
 *
 * This class can be used for 'typeless' virtual pointers, similar as void* is used
 * for 'regular' typeless pointers.
 * @sa @ref aTypeless
 */

class CBaseVirtPtr
{
    struct SNull { };

    // Safe bool idiom from boost::function
    struct SDummy { void nonNull(void) { } };
    typedef void (SDummy::*TSafeBool)(void);

    template <typename, typename> friend class CVirtPtr;
    template <typename> friend class CVirtPtrLock;

public:
    /**
      * @brief Platform dependent numeric type to store raw (virtual/regular) pointer addresses
      */
#ifdef VIRTMEM_WRAP_CPOINTERS
#if defined(__x86_64__) || defined(_M_X64)
    typedef __uint128_t TPtrNum;
#elif defined(__i386) || defined(_M_IX86)
    typedef uint64_t TPtrNum;
#else
    typedef private_utils::TConditional<(sizeof(intptr_t) > sizeof(TVirtPointer)), intptr_t, TVirtPointer>::type TPtrNum;
#endif
#else
    typedef TVirtPointer TPtrNum;
#endif

private:
#ifdef VIRTMEM_WRAP_CPOINTERS
    enum { WRAP_BIT = sizeof(TPtrNum) * 8 - 1 }; // Last bit is used to check if wrapping real pointer
#endif

protected:
    // UNDONE
    // For debug
#if defined(VIRTMEM_WRAP_CPOINTERS) && (defined(__x86_64__) || defined(_M_X64))
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

#ifdef VIRTMEM_WRAP_CPOINTERS
    // Return 'real' address of pointer, ie without wrapping bit
    // static so that CValueWrapper can use it as well
    static intptr_t getPtrNum(TPtrNum p) { return p & ~((TPtrNum)1 << WRAP_BIT); }
    static void *unwrap(TPtrNum p) { ASSERT(isWrapped(p)); return reinterpret_cast<void *>(getPtrNum(p)); }
#else
    static intptr_t getPtrNum(TPtrNum p) { return p; }
#endif
    intptr_t getPtrNum(void) const { return getPtrNum(ptr); } // Shortcut

public:
#ifdef VIRTMEM_WRAP_CPOINTERS
    /**
     * @brief Returns raw address of regular pointer wrapped by a virtual pointer.
     * @param p Numeric value of the virtual pointer
     * @sa getRawNum
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    static TPtrNum getWrapped(TPtrNum p) { return p | ((TPtrNum)1 << WRAP_BIT); }
    /**
     * @brief Returns whether a virtual pointer has wrapped a regular pointer.
     * @param p Numeric value of the virtual pointer
     * @sa getRawNum
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    static bool isWrapped(TPtrNum p) { return p & ((TPtrNum)1 << WRAP_BIT); }
    /**
     * @brief Returns whether a virtual pointer has wrapped a regular pointer (non static version).
     * @param p Numeric value of the virtual pointer
     * @sa getRawNum
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    bool isWrapped(void) const { return isWrapped(ptr); }
#endif

    /**
     * @brief Returns a numeric representation of this virtual pointer.
     *
     * This function can be used to convert virtual pointers to numeric values.
     * @note The value returned by this function depends whether this virtual pointer wraps a
     * regular pointer. If this is the case, the returned value contains the raw pointer address and
     * a flag to indicate that this is not a virtual pointer. In this case getWrapped can be used
     * to obtain the pointer address. If this virtual pointer does *not* wrap a regular pointer,
     * the value returned by this function equals a virtual pointer address that can be
     * used directly by an allocator for raw memory access.
     * @sa getWrapped, isWrapped
     */
    TPtrNum getRawNum(void) const { return ptr; }
    /**
     * @brief Sets a virtual pointer from a numeric value.
     * @param p The raw numeric representation of a virtual pointer.
     * @sa getRawNum
     */
    void setRawNum(TPtrNum p) { ptr = p; }

#ifdef VIRTMEM_WRAP_CPOINTERS
    /**
     * @brief Wraps a regular pointer
     * @sa CVirtPtr::wrap
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    static CBaseVirtPtr wrap(const void *p)
    {
        CBaseVirtPtr ret;
        ret.ptr = getWrapped(reinterpret_cast<TPtrNum>(p));
        return ret;
    }
    void *unwrap(const CBaseVirtPtr &p) { return unwrap(p); } //!< @copydoc CVirtPtr::unwrap
    void *unwrap(void) { return unwrap(ptr); } //!< @copydoc CVirtPtr::unwrap(void)
    const void *unwrap(void) const { return unwrap(ptr); } //!< @copydoc CVirtPtr::unwrap(void) const
#endif

    // HACK: this allows constructing CVirtPtr objects from CBaseVirtPtr variables, similar to
    // initializing non void pointers with a void pointer
    // Note that we could have used a copy constructor in CVirtPtr instead, but this would make the latter
    // class non-POD
    //! Conversion operator to CVirtPtr types.
    template <typename T, typename A> EXPLICIT operator CVirtPtr<T, A>(void) const { CVirtPtr<T, A> ret; ret.ptr = ptr; return ret; }

    // allow checking with NULL
    /**
     * @name Pointer validity checking operators.
     * The following operators can be used to see whether a virtual pointer is valid
     * (i.e. is not `NULL`). Overloads exist to support checking for `0`, `NULL` and \ref NILL.
     * The operators also work for wrapped regular pointers.
     * @{
     */
    inline bool operator==(const SNull *) const { return getPtrNum() == 0; }
    friend inline bool operator==(const SNull *, const CBaseVirtPtr &pw) { return pw.getPtrNum() == 0; }
    inline bool operator==(const CNILL &) const { return getPtrNum() == 0; }
    friend inline bool operator==(const CNILL &, const CBaseVirtPtr &pw) { return pw.getPtrNum() == 0; }
    inline bool operator!=(const SNull *) const { return getPtrNum() != 0; }
    friend inline bool operator!=(const SNull *, const CBaseVirtPtr &pw) { return pw.getPtrNum() != 0; }
    inline bool operator!=(const CNILL &) const { return getPtrNum() != 0; }
    friend inline bool operator!=(const CNILL &, const CBaseVirtPtr &pw) { return pw.getPtrNum() != 0; }
    //! Allows `if (myvirtptr) ...` expressions.
    inline operator TSafeBool (void) const { return getPtrNum() == 0 ? 0 : &SDummy::nonNull; }
    // @}

    /**
      * @name Pointer comparison operators.
      * The following operators are used for comparing two virtual pointers. Comparing two wrapped
      * regulared pointers is supported. Comparing wrapped and non wrapped pointers is undefined.
      * @{
      */
    // UNDONE: _int128_t comparison sometimes fails??? Perhaps only check for ptrnums/iswrapped when on x86-64? Test this!
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    inline bool operator==(const CBaseVirtPtr &pb) const { return getPtrNum() == pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
    inline bool operator!=(const CBaseVirtPtr &pb) const { return getPtrNum() != pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
    inline bool operator<(const CBaseVirtPtr &pb) const { return getPtrNum() < pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
    inline bool operator<=(const CBaseVirtPtr &pb) const { return getPtrNum() <= pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
    inline bool operator>=(const CBaseVirtPtr &pb) const { return getPtrNum() >= pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
    inline bool operator>(const CBaseVirtPtr &pb) const { return getPtrNum() > pb.getPtrNum() && isWrapped() == pb.isWrapped(); }
    // @}
#else
    inline bool operator==(const CBaseVirtPtr &pb) const { return ptr == pb.ptr; }
    inline bool operator!=(const CBaseVirtPtr &pb) const { return ptr != pb.ptr; }
    inline bool operator<(const CBaseVirtPtr &pb) const { return ptr < pb.ptr; }
    inline bool operator<=(const CBaseVirtPtr &pb) const { return ptr <= pb.ptr; }
    inline bool operator>=(const CBaseVirtPtr &pb) const { return ptr >= pb.ptr; }
    inline bool operator>(const CBaseVirtPtr &pb) const { return ptr > pb.ptr; }
#endif

};

}

#undef EXPLICIT

#endif // VIRTMEM_BASE_WRAPPER_H

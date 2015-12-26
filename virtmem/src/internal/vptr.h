#ifndef VIRTMEM_VPTR_H
#define VIRTMEM_VPTR_H

/**
  @file
  @brief This header contains the class definition of the VPtr class
  */

#include "internal/alloc.h"
#include "base_vptr.h"
#include "config/config.h"
#include "utils.h"

#include <stddef.h>

namespace virtmem {

template <typename T, typename TA> class VPtr;

namespace private_utils {

template <typename T> struct Dereferenced { typedef T type; }; // dummy for non pointers
template <typename T> struct Dereferenced<T *(*)()> { typedef T *type; }; // dummy for function pointers. // UNDONE: only handles 1 arg
template <typename T> struct Dereferenced<T *> { typedef T type; };
template <typename T, typename A> struct Dereferenced<VPtr<T, A> > { typedef T type; };

// get pointer to variable, use char & cast to override any operator & overload
template <typename T> T *pointerTo(const T &val) { return (T *)&(char &)val; }
}

/**
 * @brief Virtual pointer class that provides an interface to access virtual much like 'regular pointers'.
 *
 * This class provides an easy to use interface to access from virtual memory. The class provides
 * functionality such as dereferencing data, array access, memory (de)allocation and
 * pointer arithmetic. This class is designed in such a way that it can be
 * treated mostly as 'plain old data' (POD) and can therefore be used in an `union` as well.
 *
 * @tparam T The type of the data this pointer points to (e.g. char, int, a struct etc...)
 * @tparam TA The allocator type that contains the virtual memory pool where the pointed data resides.
 *
 * @sa BaseVPtr, TSPIRAMVirtPtr, TSDVirtPtr, TSerRAMVirtPtr, TStaticVPtr and TStdioVirtPtr
 */

template <typename T, typename TA> class VPtr : public BaseVPtr
{
public:
    typedef T *TPtr; //!< (non virtual) pointer type to the base type of this virtual pointer.
    typedef TA Allocator; //!< Allocator type used by this virtual pointer class. @sa getAlloc

private:
    typedef VPtr<T, Allocator> ThisVPtr;

    static T *read(PtrNum p)
    {
        if (!p)
            return 0;
#ifdef VIRTMEM_WRAP_CPOINTERS
        if (isWrapped(p))
            return static_cast<T *>(BaseVPtr::unwrap(p));
#endif
        return static_cast<T *>(getAlloc()->read(p, sizeof(T)));
    }
    T *read(void) const { return read(ptr); }

    static void write(PtrNum p, const T *d)
    {
#ifdef VIRTMEM_WRAP_CPOINTERS
        if (isWrapped(p))
            *(static_cast<T *>(BaseVPtr::unwrap(p))) = *d;
        else
#endif
            getAlloc()->write(p, d, sizeof(T));
    }
    void write(const T *d) { write(ptr, d); }

    ThisVPtr copy(void) const { ThisVPtr ret; ret.ptr = ptr; return ret; }
    template <typename> friend class VPtrLock;

public:
    /**
     * @brief Proxy class returned when dereferencing virtual pointers.
     * @sa @ref aAccess
     */
    class ValueWrapper
    {
        PtrNum ptr;

        ValueWrapper(PtrNum p) : ptr(p) { }
        ValueWrapper(const ValueWrapper &);

        template <typename, typename> friend class VPtr;

    public:
        /**
         * @name Proxy operators
         * @{
         */
        inline operator T(void) const { return *read(ptr); }
        template <typename T2> VIRTMEM_EXPLICIT inline operator T2(void) const { return static_cast<T2>(operator T()); }

//        ValueWrapper &operator=(const ValueWrapper &v)
        // HACK: 'allow' non const assignment of types. In reality this makes sure we don't define
        // the assignment operator twice for const types (where T == const T)
        ValueWrapper &operator=(const typename VPtr<typename private_utils::AntiConst<T>::type, TA>::ValueWrapper &v)
        {
            ASSERT(ptr != 0);
            if (ptr != v.ptr)
            {
                const T val = *read(v.ptr);
                write(ptr, &val);
            }
            return *this;
        }
        // const conversion
        ValueWrapper &operator=(const typename VPtr<const T, TA>::ValueWrapper &v)
        {
            ASSERT(ptr != 0);
            if (ptr != v.ptr)
            {
                const T val = *read(v.ptr);
                write(ptr, &val);
            }
            return *this;
        }

        inline ValueWrapper &operator=(const T &v) { write(ptr, &v); return *this; }
        inline ThisVPtr operator&(void) { ThisVPtr ret; ret.ptr = ptr; return ret; }

        // allow pointer to pointer access
        // UNDONE: use member wrapper here?
        inline T operator->(void) { return operator T(); }
        inline const T operator->(void) const { return operator T(); }
        inline typename VPtr<typename private_utils::Dereferenced<T>::type, TA>::ValueWrapper operator*(void) { return *operator T(); }
        inline const typename VPtr<typename private_utils::Dereferenced<T>::type, TA>::ValueWrapper operator*(void) const { return *operator T(); }

        typename VPtr<typename private_utils::Dereferenced<T>::type, TA>::ValueWrapper operator[](int i)
        { return operator T()[i]; }
        const typename VPtr<typename private_utils::Dereferenced<T>::type, TA>::ValueWrapper operator[](int i) const
        { return operator T()[i]; }

        template <typename T2> inline bool operator==(const T2 &v) const { return operator T() == v; }
        template <typename T2> inline bool operator!=(const T2 &v) const { return operator T() != v; }

        ValueWrapper &operator+=(int n) { T newv = operator T() + n; write(ptr, private_utils::pointerTo(newv)); return *this; }
        ValueWrapper &operator-=(int n) { return operator+=(-n); }
        ValueWrapper &operator*=(int n) { T newv = operator T() * n; write(ptr, private_utils::pointerTo(newv)); return *this; }
        ValueWrapper &operator/=(int n) { T newv = operator T() / n; write(ptr, private_utils::pointerTo(newv)); return *this; }
        ValueWrapper &operator++(void) { return operator +=(1); }
        T operator++(int) { T ret = operator T(); operator++(); return ret; }
        //! @}
    };

    // Based on Stroustrup's general wrapper paper (http://www.stroustrup.com/wrapper.pdf)
    /**
     * @brief Proxy class used when member access is requested on a virtual pointer.
     * @sa @ref aAccess
     */
    class MemberWrapper
    {
        const PtrNum ptr;

        template <typename, typename> friend class VPtr;

        MemberWrapper(PtrNum p) : ptr(p) { }
        MemberWrapper(const MemberWrapper &);

        MemberWrapper &operator=(const MemberWrapper &); // No assignment

    public:
        ~MemberWrapper(void)
        {
#ifdef VIRTMEM_WRAP_CPOINTERS
            if (!isWrapped(ptr))
#endif
                getAlloc()->releaseLock(ptr);
        }

        /**
         * @name Access operators
         * @{
         */

        T *operator->(void)
        {
#ifdef VIRTMEM_WRAP_CPOINTERS
            if (isWrapped(ptr))
                return static_cast<T *>(BaseVPtr::unwrap(ptr));
#endif
            return static_cast<T *>(getAlloc()->makeDataLock(getPtrNum(ptr), sizeof(T)));
        }
        const T *operator->(void) const
        {
#ifdef VIRTMEM_WRAP_CPOINTERS
            if (isWrapped(ptr))
                return static_cast<T *>(BaseVPtr::unwrap(ptr));
#endif
            return static_cast<T *>(getAlloc()->makeDataLock(getPtrNum(ptr), sizeof(T), true));
        }
        //! @}
    };

#ifdef VIRTMEM_CPP11
    // Can only use these constructors on C++11, otherwise vptrs cannot be used with unions
    VPtr(void) = default; // Must have this to remain union compatible
    VPtr(NILL_t) : BaseVPtr(nullptr) { } //!< Construct from NILL/nullptr (C++11 only)
#endif

#ifdef VIRTMEM_WRAP_CPOINTERS
    /**
      * @name Members related to regular pointer wrapping
      * The following functions / operators are only defined when \ref VIRTMEM_WRAP_CPOINTERS is set.
      * @{
      */

    /**
     * @brief Wraps a regular pointer.
     *
     * Regular (non virtual) pointers can be stored inside a virtual pointer (wrapping).
     * In this situation, arithmetic, access, etc. will be proxied to the regular pointer.
     * This is useful when using code that mixes both virtual and non-virtual pointers.
     *
     * Example:
     * @code
     * int data[] = { 1, 2, 3, 4 };
     * typedef virtmem::VPtr<int, SDVAlloc> vptrType;
     * vptrType vptr = vptrType::wrap(data);
     * vptr += 2;
     * Serial.println(*vptr); // prints "3"
     * @endcode
     * @param p regular pointer to wrap
     * @return A virtual pointer wrapping the specified pointer.
     * @sa unwrap
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    static ThisVPtr wrap(const T *p)
    {
        ThisVPtr ret;
        ret.ptr = getWrapped(reinterpret_cast<PtrNum>(p));
        return ret;
    }
    /**
     * @brief Provide access to wrapped regular pointer.
     * @param p virtual pointer that wraps a regular pointer
     * @return regular pointer wrapped by specified virtual pointer.
     * @sa wrap
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    static T *unwrap(const ThisVPtr &p) { return static_cast<T *>(BaseVPtr::unwrap(p)); }
    /**
     * @brief Provide access to wrapped regular pointer (non static version).
     * @sa VPtr::unwrap(const ThisVPtr &p), wrap
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    T *unwrap(void) { return static_cast<T *>(BaseVPtr::unwrap(ptr)); }
    /**
     * @brief Provide access to wrapped regular pointer (non static const version).
     * @sa VPtr::unwrap(const ThisVPtr &p), wrap
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    const T *unwrap(void) const { return static_cast<const T *>(BaseVPtr::unwrap(ptr)); }

#ifdef VIRTMEM_VIRT_ADDRESS_OPERATOR
    /**
     * @brief Returns a virtual pointer that has the memory address of this virtual pointer wrapped.
     * @note This operator is *only* overloaded when \ref VIRTMEM_WRAP_CPOINTERS and \ref VIRTMEM_VIRT_ADDRESS_OPERATOR
     * are defined (e.g. in config.h).
     * @sa \ref VIRTMEM_VIRT_ADDRESS_OPERATOR
     */
    VPtr<ThisVPtr, Allocator> operator&(void) { VPtr<ThisVPtr, Allocator> ret = ret.wrap(this); return ret; }

    /**
     * @brief Returns a regular pointer to the address of this virtual pointer
     * @note This function is only defined when \ref VIRTMEM_WRAP_CPOINTERS and \ref VIRTMEM_VIRT_ADDRESS_OPERATOR
     * are defined (e.g. in config.h).
     * @sa \ref VIRTMEM_VIRT_ADDRESS_OPERATOR
     */
    const VPtr *addressOf(void) const { return this; }
    VPtr *addressOf(void) { return this; } //!< @copydoc addressOf(void) const
#endif
    // @}

#endif

    /**
     * @name Dereference operators
     * The following operators are used for accessing the data pointed to by this virtual pointer.
     * The returned value is a proxy class, which mostly acts as the data itself.
     * @sa aAccess
     * @{
     */
    ValueWrapper operator*(void) { return ValueWrapper(ptr); }
    MemberWrapper operator->(void) { return MemberWrapper(ptr); }
    const MemberWrapper operator->(void) const { return MemberWrapper(ptr); }
    const ValueWrapper operator[](int i) const { return ValueWrapper(ptr + (i * sizeof(T))); }
    ValueWrapper operator[](int i) { return ValueWrapper(ptr + (i * sizeof(T))); }
    //! @}

    /**
     * @name Const conversion operators
     * @{
     */
    inline operator VPtr<const T, Allocator>(void) { VPtr<const T, Allocator> ret; ret.ptr = ptr; return ret; }
    // pointer to pointer conversion
    template <typename T2> VIRTMEM_EXPLICIT inline operator VPtr<T2, Allocator>(void) { VPtr<T2, Allocator> ret; ret.ptr = ptr; return ret; }
    //! @}

    /**
      * @name Pointer arithmetic
      * These operators can be used for pointer arithmetics.
      * @{
      */
    // NOTE: int cast is necessary to deal with negative numbers
    ThisVPtr &operator+=(int n) { ptr += (n * (int)sizeof(T)); return *this; }
    inline ThisVPtr &operator++(void) { return operator +=(1); }
    inline ThisVPtr operator++(int) { ThisVPtr ret = copy(); operator++(); return ret; }
    inline ThisVPtr &operator-=(int n) { return operator +=(-n); }
    inline ThisVPtr &operator--(void) { return operator -=(1); }
    inline ThisVPtr operator--(int) { ThisVPtr ret = copy(); operator--(); return ret; }
    inline ThisVPtr operator+(int n) const { return (copy() += n); }
    inline ThisVPtr operator-(int n) const { return (copy() -= n); }
    int operator-(const ThisVPtr &other) const { return (ptr - other.ptr) / sizeof(T); }
    //! @}
//    inline bool operator!=(const TVirtPtr &p) const { return ptr != p.ptr; } UNDONE: need this?

    /**
     * @brief Returns instance of allocator bound to this virtual pointer.
     * @sa VAlloc::getInstance
     */
    static inline Allocator *getAlloc(void) { return static_cast<Allocator *>(Allocator::getInstance()); }
};

}


#endif // VIRTMEM_VPTR_H

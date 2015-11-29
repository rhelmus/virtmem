#ifndef VIRTMEM_VPTR_H
#define VIRTMEM_VPTR_H

/**
  @file
  @brief This header contains the class definition of the VPtr class
  */

#include "alloc.h"
#include "base_vptr.h"
#include "utils.h"

#include <stddef.h>

#if __cplusplus > 199711L
#define EXPLICIT explicit
#else
#define EXPLICIT
#endif

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
 * pointer arithmetic.
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
    typedef VPtr<T, Allocator> TVirtPtr;

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

    TVirtPtr copy(void) const { TVirtPtr ret; ret.ptr = ptr; return ret; }
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
        inline operator T(void) const { return *read(ptr); }
        template <typename T2> inline operator T2(void) const { return static_cast<T2>(operator T()); } // UNDONE: explicit?

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
        inline TVirtPtr operator&(void) { TVirtPtr ret; ret.ptr = ptr; return ret; }

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
    };

    // C style malloc/free
    /**
     * @brief Allocates memory from the linked virtual memory allocator
     * @param size The number of bytes to allocate. By default this is the size of the type pointed to.
     * @return Virtual pointer pointing to allocated memory.
     *
     * This function is the equivelant of the C `malloc` function. To allocate memory for an
     * array multiply the array size by the size of the type, for instance:
     * @code
     * // allocate array of 10 integers.
     * vptr = virtmem::VPtr<int, SDVAlloc>::alloc(10 * sizeof(int));
     * @endcode
     * @sa free, newClass, newArray, BaseVAlloc::alloc
     */
    static TVirtPtr alloc(VPtrSize size=sizeof(T))
    {
        TVirtPtr ret;
        ret.ptr = getAlloc()->alloc(size);
        return ret;
    }

    /**
     * @brief Frees a block of virtual memory
     * @param p virtual pointer that points to block to be freed
     *
     * This function is the equivelant of the C `free` function.
     * @sa alloc, deleteClass, deleteArray, BaseVAlloc::free
     */
    static void free(TVirtPtr &p)
    {
        getAlloc()->free(p.ptr);
        p.ptr = 0;
    }

    // C++ style new/delete --> call constructors (by placement new) and destructors
    /**
     * @brief Allocates memory and constructs data type
     * @param size The number of bytes to allocate. By default this is the size of the type pointed to.
     * @return Virtual pointer pointing to allocated memory.
     *
     * This function is similar to the C++ `new` operator. Similar to \ref alloc, this function will
     * allocate a block of virtual memory. Subsequently, the default constructor of the data type is
     * called. For this reason, this function is typically used for C++ classes.
     * @note This function should be used together with \ref deleteClass.
     * @sa deleteClass, newArray, alloc
     */
    static TVirtPtr newClass(VPtrSize size=sizeof(T))
    {
        TVirtPtr ret = alloc(size);
        new (read(ret.ptr)) T; // UNDONE: can this be ro?
        return ret;
    }

    /**
     * @brief Destructs data type and frees memory
     * @param p virtual pointer to delete
     *
     * This function is similar to the C++ `delete` operator. After calling the destructor of the
     * data type the respective memory block will be freed.
     * @note This function should be used together with \ref newClass.
     * @sa newClass, deleteArray, free
     */
    static void deleteClass(TVirtPtr &p)
    {
        read(p.ptr)->~T();
        free(p);
    }

    // C++ style new []/delete [] --> calls constructors and destructors
    // the size of the array (necessary for destruction) is stored at the beginning of the block.
    // A pointer offset from this is returned.
    /**
     * @brief Allocates and constructs an array of objects.
     * @param elements the size of the array
     * @return Virtual pointer to start of the array
     *
     * This function is similar to the C++ new [] operator. After allocating sufficient size for the
     * array, the default constructor will be called for each object.
     * @note This function should be used together with \ref deleteArray.
     * @sa deleteArray, newClass, alloc
     */
    static TVirtPtr newArray(VPtrSize elements)
    {
        TVirtPtr ret = alloc(sizeof(T) * elements + sizeof(VPtrSize));
        getAlloc()->write(ret.ptr, &elements, sizeof(VPtrSize));
        ret.ptr += sizeof(VPtrSize);
        for (VPtrSize s=0; s<elements; ++s)
            new (getAlloc()->read(ret.ptr + (s * sizeof(T)), sizeof(T))) T; // UNDONE: can this be ro?
        return ret;
    }
    /**
     * @brief Destructs and frees an array of objects.
     * @param p Virtual pointer to array that should be deleted
     *
     * This function is similar to the C++ `delete []` operator. After calling all the
     * destructors of each object, the respective memory block will be freed.
     * @note This function should be used together with \ref newArray.
     * @sa newArray, deleteClass, free
     */
    static void deleteArray(TVirtPtr &p)
    {
        const PtrNum soffset = p.ptr - sizeof(VPtrSize); // pointer to size offset
        VPtrSize *size = static_cast<VPtrSize *>(getAlloc()->read(soffset, sizeof(VPtrSize)));
        for (VPtrSize s=0; s<*size; ++s)
            (read(p.ptr + (s * sizeof(T))))->~T();
        getAlloc()->free(soffset); // soffset points at beginning of actual block
    }

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
    static TVirtPtr wrap(const T *p)
    {
        TVirtPtr ret;
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
    static T *unwrap(const TVirtPtr &p) { return static_cast<T *>(BaseVPtr::unwrap(p)); }
    /**
     * @brief Provide access to wrapped regular pointer (non static version).
     * @sa VPtr::unwrap(const TVirtPtr &p), wrap
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    T *unwrap(void) { return static_cast<T *>(BaseVPtr::unwrap(ptr)); }
    /**
     * @brief Provide access to wrapped regular pointer (non static const version).
     * @sa VPtr::unwrap(const TVirtPtr &p), wrap
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
    VPtr<TVirtPtr, Allocator> operator&(void) { VPtr<TVirtPtr, Allocator> ret = ret.wrap(this); return ret; }

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
    // @}

    // const conversion
    inline operator VPtr<const T, Allocator>(void) { VPtr<const T, Allocator> ret; ret.ptr = ptr; return ret; }
    // pointer to pointer conversion
    template <typename T2> EXPLICIT inline operator VPtr<T2, Allocator>(void) { VPtr<T2, Allocator> ret; ret.ptr = ptr; return ret; }

    /**
      * @name Pointer arithmetic
      * These operators can be used for pointer arithmetics.
      * @{
      */
    // NOTE: int cast is necessary to deal with negative numbers
    TVirtPtr &operator+=(int n) { ptr += (n * (int)sizeof(T)); return *this; }
    inline TVirtPtr &operator++(void) { return operator +=(1); }
    inline TVirtPtr operator++(int) { TVirtPtr ret = copy(); operator++(); return ret; }
    inline TVirtPtr &operator-=(int n) { return operator +=(-n); }
    inline TVirtPtr &operator--(void) { return operator -=(1); }
    inline TVirtPtr operator--(int) { TVirtPtr ret = copy(); operator--(); return ret; }
    inline TVirtPtr operator+(int n) const { return (copy() += n); }
    inline TVirtPtr operator-(int n) const { return (copy() -= n); }
    int operator-(const TVirtPtr &other) const { return (ptr - other.ptr) / sizeof(T); }
    // @}
//    inline bool operator!=(const TVirtPtr &p) const { return ptr != p.ptr; } UNDONE: need this?

    /**
     * @brief Returns instance of allocator bound to this virtual pointer.
     * @sa VAlloc::getInstance
     */
    static inline Allocator *getAlloc(void) { return static_cast<Allocator *>(Allocator::getInstance()); }
};

}

#undef EXPLICIT

#endif // VIRTMEM_VPTR_H
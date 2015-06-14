#ifndef VIRTMEM_WRAPPER_H
#define VIRTMEM_WRAPPER_H

#include "alloc.h"
#include "base_wrapper.h"
#include "utils.h"

#include <stddef.h>

#if __cplusplus > 199711L
#define EXPLICIT explicit
#else
#define EXPLICIT
#endif

namespace virtmem {

template <typename T, typename TA> class CVirtPtr;

namespace private_utils {

template <typename T> struct TDereferenced { typedef T type; }; // dummy for non pointers
template <typename T> struct TDereferenced<T *(*)()> { typedef T *type; }; // dummy for function pointers. // UNDONE: only handles 1 arg
template <typename T> struct TDereferenced<T *> { typedef T type; };
template <typename T, typename A> struct TDereferenced<CVirtPtr<T, A> > { typedef T type; };

// get pointer to variable, use char & cast to override any operator & overload
template <typename T> T *pointerTo(const T &val) { return (T *)&(char &)val; }
}

template <typename T, typename TA> class CVirtPtr : public CBaseVirtPtr
{
public:
    typedef T *TPtr; //!< (non virtual) pointer type to the base type of this virtual pointer.
    typedef TA TAllocator; //!< Allocator type used by this virtual pointer class. @sa getAlloc

private:
    typedef CVirtPtr<T, TAllocator> TVirtPtr;

    static T *read(TPtrNum p)
    {
        if (!p)
            return 0;
#ifdef VIRTMEM_WRAP_CPOINTERS
        if (isWrapped(p))
            return static_cast<T *>(CBaseVirtPtr::unwrap(p));
#endif
        return static_cast<T *>(getAlloc()->read(p, sizeof(T)));
    }
    T *read(void) const { return read(ptr); }

    static void write(TPtrNum p, const T *d)
    {
#ifdef VIRTMEM_WRAP_CPOINTERS
        if (isWrapped(p))
            *(static_cast<T *>(CBaseVirtPtr::unwrap(p))) = *d;
        else
#endif
            getAlloc()->write(p, d, sizeof(T));
    }
    void write(const T *d) { write(ptr, d); }

    TVirtPtr copy(void) const { TVirtPtr ret; ret.ptr = ptr; return ret; }
    template <typename> friend class CPtrWrapLock;

public:
    class CValueWrapper
    {
        TPtrNum ptr;

        CValueWrapper(TPtrNum p) : ptr(p) { }
        CValueWrapper(const CValueWrapper &);

        template <typename, typename> friend class CVirtPtr;

    public:
        inline operator T(void) const { return *read(ptr); }
        template <typename T2> inline operator T2(void) const { return static_cast<T2>(operator T()); } // UNDONE: explicit?

//        CValueWrapper &operator=(const CValueWrapper &v)
        // HACK: 'allow' non const assignment of types. In reality this makes sure we don't define
        // the assignment operator twice for const types (where T == const T)
        CValueWrapper &operator=(const typename CVirtPtr<typename private_utils::TAntiConst<T>::type, TA>::CValueWrapper &v)
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
        CValueWrapper &operator=(const typename CVirtPtr<const T, TA>::CValueWrapper &v)
        {
            ASSERT(ptr != 0);
            if (ptr != v.ptr)
            {
                const T val = *read(v.ptr);
                write(ptr, &val);
            }
            return *this;
        }

        inline CValueWrapper &operator=(const T &v) { write(ptr, &v); return *this; }
        inline TVirtPtr operator&(void) { TVirtPtr ret; ret.ptr = ptr; return ret; }

        // allow pointer to pointer access
        // UNDONE: use member wrapper here?
        inline T operator->(void) { return operator T(); }
        inline const T operator->(void) const { return operator T(); }
        inline typename CVirtPtr<typename private_utils::TDereferenced<T>::type, TA>::CValueWrapper operator*(void) { return *operator T(); }
        inline const typename CVirtPtr<typename private_utils::TDereferenced<T>::type, TA>::CValueWrapper operator*(void) const { return *operator T(); }

        typename CVirtPtr<typename private_utils::TDereferenced<T>::type, TA>::CValueWrapper operator[](int i)
        { return operator T()[i]; }
        const typename CVirtPtr<typename private_utils::TDereferenced<T>::type, TA>::CValueWrapper operator[](int i) const
        { return operator T()[i]; }

        template <typename T2> inline bool operator==(const T2 &v) const { return operator T() == v; }
        template <typename T2> inline bool operator!=(const T2 &v) const { return operator T() != v; }

        CValueWrapper &operator+=(int n) { T newv = operator T() + n; write(ptr, private_utils::pointerTo(newv)); return *this; }
        CValueWrapper &operator-=(int n) { return operator+=(-n); }
        CValueWrapper &operator*=(int n) { T newv = operator T() * n; write(ptr, private_utils::pointerTo(newv)); return *this; }
        CValueWrapper &operator/=(int n) { T newv = operator T() / n; write(ptr, private_utils::pointerTo(newv)); return *this; }
        CValueWrapper &operator++(void) { return operator +=(1); }
        T operator++(int) { T ret = operator T(); operator++(); return ret; }
    };

    // Based on Strousstrup's general wrapper paper (http://www.stroustrup.com/wrapper.pdf)
    class CMemberWrapper
    {
        const TPtrNum ptr;

        template <typename, typename> friend class CVirtPtr;

        CMemberWrapper(TPtrNum p) : ptr(p) { }
        CMemberWrapper(const CMemberWrapper &);

        CMemberWrapper &operator=(const CMemberWrapper &); // No assignment

    public:
        ~CMemberWrapper(void)
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
                return static_cast<T *>(CBaseVirtPtr::unwrap(ptr));
#endif
            return static_cast<T *>(getAlloc()->makeDataLock(getPtrNum(ptr), sizeof(T)));
        }
        const T *operator->(void) const
        {
#ifdef VIRTMEM_WRAP_CPOINTERS
            if (isWrapped(ptr))
                return static_cast<T *>(CBaseVirtPtr::unwrap(ptr));
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
     * vptr = virtmem::CVirtPtr<int, CSdfatlibVirtMemAlloc>::alloc(10 * sizeof(int));
     * @endcode
     * @sa free, newClass, newArray, CBaseVirtMemAlloc::alloc
     */
    static TVirtPtr alloc(TVirtPtrSize size=sizeof(T))
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
     * @sa alloc, deleteClass, deleteArray, CBaseVirtMemAlloc::free
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
    static TVirtPtr newClass(TVirtPtrSize size=sizeof(T))
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
    static TVirtPtr newArray(TVirtPtrSize elements)
    {
        TVirtPtr ret = alloc(sizeof(T) * elements + sizeof(TVirtPtrSize));
        getAlloc()->write(ret.ptr, &elements, sizeof(TVirtPtrSize));
        ret.ptr += sizeof(TVirtPtrSize);
        for (TVirtPtrSize s=0; s<elements; ++s)
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
        const TPtrNum soffset = p.ptr - sizeof(TVirtPtrSize); // pointer to size offset
        TVirtPtrSize *size = static_cast<TVirtPtrSize *>(getAlloc()->read(soffset, sizeof(TVirtPtrSize)));
        for (TVirtPtrSize s=0; s<*size; ++s)
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
     * typedef virtmem::CVirtPtr<int, CSdfatlibVirtMemAlloc> vptrType;
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
        ret.ptr = getWrapped(reinterpret_cast<TPtrNum>(p));
        return ret;
    }
    /**
     * @brief Provide access to wrapped regular pointer.
     * @param p virtual pointer that wraps a regular pointer
     * @return regular pointer wrapped by specified virtual pointer.
     * @sa wrap
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    static T *unwrap(const TVirtPtr &p) { return static_cast<T *>(CBaseVirtPtr::unwrap(p)); }
    /**
     * @brief Provide access to wrapped regular pointer (non static version).
     * @sa CVirtPtr::unwrap(const TVirtPtr &p), wrap
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    T *unwrap(void) { return static_cast<T *>(CBaseVirtPtr::unwrap(ptr)); }
    /**
     * @brief Provide access to wrapped regular pointer (non static const version).
     * @sa CVirtPtr::unwrap(const TVirtPtr &p), wrap
     * @note \ref VIRTMEM_WRAP_CPOINTERS needs to be defined (e.g. in config.h) to enable this function.
     */
    const T *unwrap(void) const { return static_cast<const T *>(CBaseVirtPtr::unwrap(ptr)); }

#ifdef VIRTMEM_VIRT_ADDRESS_OPERATOR
    /**
     * @brief Returns a virtual pointer that has the memory address of this virtual pointer wrapped.
     * @note This operator is *only* overloaded when \ref VIRTMEM_WRAP_CPOINTERS and \ref VIRTMEM_VIRT_ADDRESS_OPERATOR
     * are defined (e.g. in config.h).
     * @sa \ref VIRTMEM_VIRT_ADDRESS_OPERATOR
     */
    CVirtPtr<TVirtPtr, TAllocator> operator&(void) { CVirtPtr<TVirtPtr, TAllocator> ret = ret.wrap(this); return ret; }

    /**
     * @brief Returns a regular pointer to the address of this virtual pointer
     * @note This function is only defined when \ref VIRTMEM_WRAP_CPOINTERS and \ref VIRTMEM_VIRT_ADDRESS_OPERATOR
     * are defined (e.g. in config.h).
     * @sa \ref VIRTMEM_VIRT_ADDRESS_OPERATOR
     */
    const CVirtPtr *addressOf(void) const { return this; }
    CVirtPtr *addressOf(void) { return this; } //!< @copydoc addressOf(void) const
#endif
    // @}

#endif

    /**
     * @name Dereference operators
     * The following operators are used for accessing the data pointed to by this virtual pointer.
     * The returned value is a proxy class, which mostly acts as the data itself. For more information, see
     * \ref ValueWrapping.
     * @{
     */
    CValueWrapper operator*(void) { return CValueWrapper(ptr); }
    CMemberWrapper operator->(void) { return CMemberWrapper(ptr); }
    const CMemberWrapper operator->(void) const { return CMemberWrapper(ptr); }
    const CValueWrapper operator[](int i) const { return CValueWrapper(ptr + (i * sizeof(T))); }
    CValueWrapper operator[](int i) { return CValueWrapper(ptr + (i * sizeof(T))); }
    // @}

    // const conversion
    inline operator CVirtPtr<const T, TAllocator>(void) { CVirtPtr<const T, TAllocator> ret; ret.ptr = ptr; return ret; }
    // pointer to pointer conversion
    template <typename T2> EXPLICIT inline operator CVirtPtr<T2, TAllocator>(void) { CVirtPtr<T2, TAllocator> ret; ret.ptr = ptr; return ret; }

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
     * @sa CVirtMemAlloc::getInstance
     */
    static inline TAllocator *getAlloc(void) { return static_cast<TAllocator *>(TAllocator::getInstance()); }
};

}

#undef EXPLICIT

#endif // VIRTMEM_WRAPPER_H

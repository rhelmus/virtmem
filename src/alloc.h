#ifndef VIRTMEM_ALLOC_H
#define VIRTMEM_ALLOC_H

/**
  @file
  @brief virtual memory class header
*/

#include "base_alloc.h"
#include "vptr.h"

namespace virtmem {

template <typename, typename> class VPtr;

/**
 * @brief Base template class for virtual memory allocators.
 *
 * This template class is used as parent class for all allocators. Most of the actual code is
 * defined in BaseVAlloc, while this class only contains code dependent upon template
 * parameters (i.e. page settings).
 *
 * @tparam Properties Allocator properties, see DefaultAllocProperties
 * @tparam Derived Dummy parameter, used to create unique singleton instances for each derived class.
 */
template <typename Properties, typename Derived>
class VAlloc : public BaseVAlloc
{
    LockPage smallPagesData[Properties::smallPageCount];
    LockPage mediumPagesData[Properties::mediumPageCount];
    LockPage bigPagesData[Properties::bigPageCount];
#ifdef NVALGRIND
    uint8_t smallPagePool[Properties::smallPageCount * Properties::smallPageSize] __attribute__ ((aligned (sizeof(TAlign))));
    uint8_t mediumPagePool[Properties::mediumPageCount * Properties::mediumPageSize] __attribute__ ((aligned (sizeof(TAlign))));
    uint8_t bigPagePool[Properties::bigPageCount * Properties::bigPageSize] __attribute__ ((aligned (sizeof(TAlign))));
#else
    uint8_t smallPagePool[Properties::smallPageCount * (Properties::smallPageSize + valgrindPad * 2)];
    uint8_t mediumPagePool[Properties::mediumPageCount * (Properties::mediumPageSize + valgrindPad * 2)];
    uint8_t bigPagePool[Properties::bigPageCount * (Properties::bigPageSize + valgrindPad * 2)];
#endif
    static VAlloc *instance;

protected:
    VAlloc(void)
    {
        ASSERT(!instance);
        instance = this;
#ifdef NVALGRIND
        initSmallPages(smallPagesData, &smallPagePool[0], Properties::smallPageCount, Properties::smallPageSize);
        initMediumPages(mediumPagesData, &mediumPagePool[0], Properties::mediumPageCount, Properties::mediumPageSize);
        initBigPages(bigPagesData, &bigPagePool[0], Properties::bigPageCount, Properties::bigPageSize);
#else
        initSmallPages(smallPagesData, &smallPagePool[pad], Properties::smallPageCount, Properties::smallPageSize);
        initMediumPages(mediumPagesData, &mediumPagePool[pad], Properties::mediumPageCount, Properties::mediumPageSize);
        initBigPages(bigPagesData, &bigPagePool[pad], Properties::bigPageCount, Properties::bigPageSize);
        VALGRIND_MAKE_MEM_NOACCESS(&smallPagePool[0], pad); VALGRIND_MAKE_MEM_NOACCESS(&smallPagePool[Properties::smallPageCount * Properties::smallPageSize + pad], pad);
        VALGRIND_MAKE_MEM_NOACCESS(&mediumPagePool[0], pad); VALGRIND_MAKE_MEM_NOACCESS(&mediumPagePool[Properties::mediumPageCount * Properties::mediumPageSize + pad], pad);
        VALGRIND_MAKE_MEM_NOACCESS(&bigPagePool[0], pad); VALGRIND_MAKE_MEM_NOACCESS(&bigPagePool[Properties::bigPageCount * Properties::bigPageSize + pad], pad);
#endif
    }
    ~VAlloc(void) { instance = 0; }

public:
    /**
     * @brief Returns a pointer to the instance of the class.
     *
     * All allocator classes are singletons: only one instance can exist for a particular allocator.
     *
     * Note that, since allocators are template classes, one instance is generated for every set of
     * unique template parameters. For example:
     * @code{.cpp}
     * virtmem::SDVAlloc alloc1; // allocator with default template parameters
     * virtmem::SDVAlloc<mysettings> alloc2; // allocator with custom template parameter
     * @endcode
     * In this case, `alloc1` and `alloc2` are variables with a *different* type, hence getInstance()
     * will return a different instance for both classes.
     */
    static VAlloc *getInstance(void) { return instance; }

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
    template <typename T> VPtr<T, Derived> alloc(VPtrSize size=sizeof(T))
    {
        virtmem::VPtr<T, Derived> ret;
        ret.setRawNum(allocRaw(size));
        return ret;
    }

    /**
     * @brief Frees a block of virtual memory
     * @param p virtual pointer that points to block to be freed
     *
     * This function is the equivelant of the C `free` function. The pointer will be set to null.
     * @sa alloc, deleteClass, deleteArray, BaseVAlloc::free
     */
    template <typename T> void free(VPtr<T, Derived> &p)
    {
        freeRaw(p.getRawNum());
        p.setRawNum(0);
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
    template <typename T> VPtr<T, Derived> newClass(VPtrSize size=sizeof(T))
    {
        virtmem::VPtr<T, Derived> ret = alloc<T>(size);
        T *ptr = static_cast<T *>(read(ret.getRawNum(), sizeof(T)));
        new (ptr) T; // UNDONE: can this be ro?
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
    template <typename T> void deleteClass(VPtr<T, Derived> &p)
    {
        p->~T();
        freeRaw(p.getRawNum());
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
    template <typename T> VPtr<T, Derived> newArray(VPtrSize elements)
    {
        VPtrNum p = allocRaw(sizeof(T) * elements + sizeof(VPtrSize));
        write(p, &elements, sizeof(VPtrSize));
        p += sizeof(VPtrSize);
        for (VPtrSize s=0; s<elements; ++s)
            new (read(p + (s * sizeof(T)), sizeof(T))) T; // UNDONE: can this be ro?

        virtmem::VPtr<T, Derived> ret;
        ret.setRawNum(p);
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
    template <typename T> void deleteArray(VPtr<T, Derived> &p)
    {
        const VPtrNum soffset = p.getRawNum() - sizeof(VPtrSize); // pointer to size offset
        VPtrSize *size = static_cast<VPtrSize *>(read(soffset, sizeof(VPtrSize)));
        for (VPtrSize s=0; s<*size; ++s)
        {
            T *ptr = static_cast<T *>(read(p.getRawNum() + (s * sizeof(T)), sizeof(T)));
            ptr->~T();
        }
        freeRaw(soffset); // soffset points at beginning of actual block
    }

    template <typename T> struct VPtr { typedef virtmem::VPtr<T, Derived> type; };
};

template <typename Properties, typename Derived>
VAlloc<Properties, Derived> *VAlloc<Properties, Derived>::instance = 0;

}

#endif // VIRTMEM_ALLOC_H

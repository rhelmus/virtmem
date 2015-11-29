#ifndef VIRTMEM_ALLOC_H
#define VIRTMEM_ALLOC_H

/**
  @file
  @brief virtual memory class header
*/

#include "base_alloc.h"

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

    // UNDONE
    template <typename T> VPtr<T, Derived> allocVPtr(VPtrSize size=sizeof(T))
    {
        return virtmem::VPtr<T, Derived>::alloc(size);
    }

    template <typename T> struct VPtr { typedef virtmem::VPtr<T, Derived> type; };
};

template <typename Properties, typename Derived>
VAlloc<Properties, Derived> *VAlloc<Properties, Derived>::instance = 0;

}

#endif // VIRTMEM_ALLOC_H

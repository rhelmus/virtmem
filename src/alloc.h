#ifndef VIRTMEM_ALLOC_H
#define VIRTMEM_ALLOC_H

/**
  @file
  @brief virtual memory class header
*/

#include "base_alloc.h"

namespace virtmem {

/**
 * @brief Base template class for virtual memory allocators.
 *
 * This template class is used as parent class for all allocators. Most of the actual code is
 * defined in CBaseVirtMemAlloc, while this class only contains code dependent upon template
 * parameters (i.e. page settings).
 *
 * @tparam TProperties Allocator properties, see SDefaultAllocProperties
 * @tparam TDerived Dummy parameter, used to create unique singleton instances for each derived class.
 */
template <typename TProperties, typename TDerived>
class CVirtMemAlloc : public CBaseVirtMemAlloc
{
    SLockPage smallPagesData[TProperties::smallPageCount];
    SLockPage mediumPagesData[TProperties::mediumPageCount];
    SLockPage bigPagesData[TProperties::bigPageCount];
#ifdef NVALGRIND
    uint8_t smallPagePool[TProperties::smallPageCount * TProperties::smallPageSize] __attribute__ ((aligned (sizeof(TAlign))));
    uint8_t mediumPagePool[TProperties::mediumPageCount * TProperties::mediumPageSize] __attribute__ ((aligned (sizeof(TAlign))));
    uint8_t bigPagePool[TProperties::bigPageCount * TProperties::bigPageSize] __attribute__ ((aligned (sizeof(TAlign))));
#else
    uint8_t smallPagePool[TProperties::smallPageCount * (TProperties::smallPageSize + valgrindPad * 2)];
    uint8_t mediumPagePool[TProperties::mediumPageCount * (TProperties::mediumPageSize + valgrindPad * 2)];
    uint8_t bigPagePool[TProperties::bigPageCount * (TProperties::bigPageSize + valgrindPad * 2)];
#endif
    static CVirtMemAlloc *instance;

protected:
    CVirtMemAlloc(void)
    {
        ASSERT(!instance);
        instance = this;
#ifdef NVALGRIND
        initSmallPages(smallPagesData, &smallPagePool[0], TProperties::smallPageCount, TProperties::smallPageSize);
        initMediumPages(mediumPagesData, &mediumPagePool[0], TProperties::mediumPageCount, TProperties::mediumPageSize);
        initBigPages(bigPagesData, &bigPagePool[0], TProperties::bigPageCount, TProperties::bigPageSize);
#else
        initSmallPages(smallPagesData, &smallPagePool[pad], TProperties::smallPageCount, TProperties::smallPageSize);
        initMediumPages(mediumPagesData, &mediumPagePool[pad], TProperties::mediumPageCount, TProperties::mediumPageSize);
        initBigPages(bigPagesData, &bigPagePool[pad], TProperties::bigPageCount, TProperties::bigPageSize);
        VALGRIND_MAKE_MEM_NOACCESS(&smallPagePool[0], pad); VALGRIND_MAKE_MEM_NOACCESS(&smallPagePool[TProperties::smallPageCount * TProperties::smallPageSize + pad], pad);
        VALGRIND_MAKE_MEM_NOACCESS(&mediumPagePool[0], pad); VALGRIND_MAKE_MEM_NOACCESS(&mediumPagePool[TProperties::mediumPageCount * TProperties::mediumPageSize + pad], pad);
        VALGRIND_MAKE_MEM_NOACCESS(&bigPagePool[0], pad); VALGRIND_MAKE_MEM_NOACCESS(&bigPagePool[TProperties::bigPageCount * TProperties::bigPageSize + pad], pad);
#endif
    }
    ~CVirtMemAlloc(void) { instance = 0; }

public:
    /**
     * @brief Returns a pointer to the instance of the class.
     *
     * All allocator classes are singletons: only one instance can exist for a particular allocator.
     *
     * Note that, since allocators are template classes, one instance is generated for every set of
     * unique template parameters. For example:
     * @code{.cpp}
     * virtmem::CSDVirtMemAlloc alloc1; // allocator with default template parameters
     * virtmem::CSDVirtMemAlloc<mysettings> alloc2; // allocator with custom template parameter
     * @endcode
     * In this case, `alloc1` and `alloc2` are variables with a *different* type, hence getInstance()
     * will return a different instance for both classes.
     */
    static CVirtMemAlloc *getInstance(void) { return instance; }
};

template <typename TProperties, typename TDerived>
CVirtMemAlloc<TProperties, TDerived> *CVirtMemAlloc<TProperties, TDerived>::instance = 0;

}

#endif // VIRTMEM_ALLOC_H

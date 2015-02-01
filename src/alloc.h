#ifndef ALLOC_H
#define ALLOC_H

#include "base_alloc.h"

// TDerived is necessary to ensure unique instance variables
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
    CVirtMemAlloc(void) : CBaseVirtMemAlloc(TProperties::poolSize)

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
    static CVirtMemAlloc *getInstance(void) { return instance; }
};

template <typename TProperties, typename TDerived>
CVirtMemAlloc<TProperties, TDerived> *CVirtMemAlloc<TProperties, TDerived>::instance = 0;


#endif // ALLOC_H

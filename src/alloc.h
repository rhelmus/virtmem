#ifndef ALLOC_H
#define ALLOC_H

#include "base_alloc.h"

#include <assert.h>

// TDerived is necessary to ensure unique instance variables
template <TVirtPtrSize POOL_SIZE, TVirtPtrSize PAGE_SIZE, uint8_t PAGE_COUNT, typename TDerived>
class CVirtMemAlloc : public CBaseVirtMemAlloc
{
    // UNDONE
    enum { SMALL_PAGECOUNT = 1, SMALL_PAGESIZE = 32, MEDIUM_PAGECOUNT = 4, MEDIUM_PAGESIZE = 128, BIG_PAGECOUNT = 4, BIG_PAGESIZE = 512 };

    SPartialLockPage smallPagesData[SMALL_PAGECOUNT], mediumPagesData[MEDIUM_PAGECOUNT], bigPagesData[BIG_PAGECOUNT];
    uint8_t smallPagePool[SMALL_PAGECOUNT * SMALL_PAGESIZE], mediumPagePool[MEDIUM_PAGECOUNT * MEDIUM_PAGESIZE], bigPagePool[BIG_PAGECOUNT * BIG_PAGESIZE];
    static CVirtMemAlloc *instance;

protected:
    CVirtMemAlloc(void) : CBaseVirtMemAlloc(POOL_SIZE)

    {
        assert(!instance);
        instance = this;
        initSmallPages(smallPagesData, &smallPagePool[0], SMALL_PAGECOUNT, SMALL_PAGESIZE);
        initMediumPages(mediumPagesData, &mediumPagePool[0], MEDIUM_PAGECOUNT, MEDIUM_PAGESIZE);
        initBigPages(bigPagesData, &bigPagePool[0], BIG_PAGECOUNT, BIG_PAGESIZE);
    }
    ~CVirtMemAlloc(void) { instance = 0; }

public:
    static CVirtMemAlloc *getInstance(void) { return instance; }
};

template <TVirtPtrSize POOL_SIZE, TVirtPtrSize PAGE_SIZE, uint8_t PAGE_COUNT, typename TDerived>
CVirtMemAlloc<POOL_SIZE, PAGE_SIZE, PAGE_COUNT, TDerived> *CVirtMemAlloc<POOL_SIZE, PAGE_SIZE, PAGE_COUNT, TDerived>::instance = 0;


#endif // ALLOC_H

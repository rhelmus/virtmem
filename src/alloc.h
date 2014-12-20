#ifndef ALLOC_H
#define ALLOC_H

#include "base_alloc.h"

#include <assert.h>

// TDerived is necessary to ensure unique instance variables
template <TVirtPtrSize POOL_SIZE, TVirtPtrSize PAGE_SIZE, uint8_t PAGE_COUNT, typename TDerived>
class CVirtMemAlloc : public CBaseVirtMemAlloc
{
    SMemPage memPages[PAGE_COUNT];
    uint8_t memPagePools[PAGE_COUNT][PAGE_SIZE];
    static CVirtMemAlloc *instance;

protected:
    CVirtMemAlloc(void) : CBaseVirtMemAlloc(memPages, PAGE_COUNT, POOL_SIZE, PAGE_SIZE)

    {
        assert(!instance);
        instance = this;

        for (uint8_t i=0; i<PAGE_COUNT; ++i)
            memPages[i].pool = &memPagePools[i][0];
    }
    ~CVirtMemAlloc(void) { instance = 0; }

public:
    static CVirtMemAlloc *getInstance(void) { return instance; }
};

template <TVirtPtrSize POOL_SIZE, TVirtPtrSize PAGE_SIZE, uint8_t PAGE_COUNT, typename TDerived>
CVirtMemAlloc<POOL_SIZE, PAGE_SIZE, PAGE_COUNT, TDerived> *CVirtMemAlloc<POOL_SIZE, PAGE_SIZE, PAGE_COUNT, TDerived>::instance = 0;


#endif // ALLOC_H

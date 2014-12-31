#ifndef SDFATLIB_ALLOC_H
#define SDFATLIB_ALLOC_H

#include "alloc.h"

template <TVirtPtrSize, TVirtPtrSize, uint8_t> class CSdfatlibVirtMemAlloc;

template <TVirtPtrSize POOL_SIZE = 1024*1024, TVirtPtrSize PAGE_SIZE=256, uint8_t PAGE_COUNT=4>
class CSdfatlibVirtMemAlloc : public CVirtMemAlloc<POOL_SIZE, PAGE_SIZE, PAGE_COUNT,
        CSdfatlibVirtMemAlloc<POOL_SIZE, PAGE_SIZE, PAGE_COUNT> >
{
    void doStart(void)
    {
    }

    void doSuspend(void) { }
    void doStop(void) {  }
    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    { }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    { }

public:
    CSdfatlibVirtMemAlloc(void) { }
    ~CSdfatlibVirtMemAlloc(void) { doStop(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TSdfatlibVirtPtr { typedef CVirtPtr<T, CSdfatlibVirtMemAlloc<> > type; };

#endif // SDFATLIB_ALLOC_H

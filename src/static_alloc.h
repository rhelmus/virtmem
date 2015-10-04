#ifndef VIRTMEM_STATIC_ALLOC_H
#define VIRTMEM_STATIC_ALLOC_H

/**
  * @file
  * @brief This file contains the static virtual memory allocator (for debug)
  */

// allocator using static array; for testing

#include <string.h>

#include "alloc.h"

namespace virtmem {

template <uint32_t poolSize=DEFAULT_POOLSIZE, typename TProperties=SDefaultAllocProperties>
class CStaticVirtMemAlloc : public CVirtMemAlloc<TProperties, CStaticVirtMemAlloc<poolSize, TProperties> >
{
    char staticData[poolSize];

    void doStart(void) { }
    void doSuspend(void) { }
    void doStop(void) { }

    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        ::memcpy(data, &staticData[offset], size);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        ::memcpy(&staticData[offset], data, size);
    }

    using CBaseVirtMemAlloc::setPoolSize;

public:
    CStaticVirtMemAlloc(void) { this->setPoolSize(poolSize); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TStaticVirtPtr { typedef CVirtPtr<T, CStaticVirtMemAlloc<> > type; };

}

#endif // VIRTMEM_STATIC_ALLOC_H

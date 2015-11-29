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

template <uint32_t poolSize=DEFAULT_POOLSIZE, typename Properties=DefaultAllocProperties>
class StaticVAlloc : public VAlloc<Properties, StaticVAlloc<poolSize, Properties> >
{
    char staticData[poolSize];

    void doStart(void) { }
    void doSuspend(void) { }
    void doStop(void) { }

    void doRead(void *data, VPtrSize offset, VPtrSize size)
    {
        ::memcpy(data, &staticData[offset], size);
    }

    void doWrite(const void *data, VPtrSize offset, VPtrSize size)
    {
        ::memcpy(&staticData[offset], data, size);
    }

    using BaseVAlloc::setPoolSize;

public:
    StaticVAlloc(void) { this->setPoolSize(poolSize); }
};

template <typename, typename> class VPtr;
template <typename T> struct TStaticVPtr { typedef VPtr<T, StaticVAlloc<> > type; };

}

#endif // VIRTMEM_STATIC_ALLOC_H

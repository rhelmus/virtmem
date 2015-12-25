#ifndef VIRTMEM_STATIC_ALLOC_H
#define VIRTMEM_STATIC_ALLOC_H

/**
  * @file
  * @brief This file contains the static virtual memory allocator (for debug)
  */

// allocator using static array; for testing

#include <string.h>

#include "internal/alloc.h"

namespace virtmem {

/**
 * @brief Virtual memory allocator that uses a static array (in regular RAM) as memory pool.
 *
 * This allocator does not have any dependencies and is mainly provided for testing.
 *
 * @tparam poolSize The size of the memory pool.
 * @tparam Properties Allocator properties, see DefaultAllocProperties
 *
 * @note Since the pool size must be given when instantiating the allocator, the @ref setPoolSize
 * function is not available for this allocator.
 *
 * @sa @ref bUsing
 */

template <uint32_t poolSize=VIRTMEM_DEFAULT_POOLSIZE, typename Properties=DefaultAllocProperties>
class StaticVAllocP : public VAlloc<Properties, StaticVAllocP<poolSize, Properties> >
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
    StaticVAllocP(void) { this->setPoolSize(poolSize); } //!< Constructs (but not initializes) the allocator.
};

typedef StaticVAllocP<> StaticVAlloc; //!< Shortcut to StaticVAllocP with default template arguments

}

#endif // VIRTMEM_STATIC_ALLOC_H

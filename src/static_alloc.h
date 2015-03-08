#ifndef VIRTMEM_STATIC_ALLOC_H
#define VIRTMEM_STATIC_ALLOC_H

// allocator using static array; for testing

#include "alloc.h"

// UNDONE: settings below
struct SStaticAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 32;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 64;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 1024;
    static const uint32_t poolSize = 1024 * 32;
};

template <typename TProperties=SStaticAllocProperties>
class CStaticVirtMemAlloc : public CVirtMemAlloc<TProperties, CStaticVirtMemAlloc<TProperties> >
{
    char staticData[TProperties::poolSize];

    void doStart(void) { }
    void doSuspend(void) { }
    void doStop(void) { }

    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        memcpy(data, &staticData[offset], size);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        memcpy(&staticData[offset], data, size);
    }

public:
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TStaticVirtPtr { typedef CVirtPtr<T, CStaticVirtMemAlloc<> > type; };


#endif // VIRTMEM_STATIC_ALLOC_H

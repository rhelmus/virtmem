#ifndef SDFATLIB_ALLOC_H
#define SDFATLIB_ALLOC_H

#include "alloc.h"

#include <SdFat.h>

template <TVirtPtrSize, TVirtPtrSize, uint8_t> class CSdfatlibVirtMemAlloc;

template <TVirtPtrSize POOL_SIZE = 1024*1024, TVirtPtrSize PAGE_SIZE=256, uint8_t PAGE_COUNT=4>
class CSdfatlibVirtMemAlloc : public CVirtMemAlloc<POOL_SIZE, PAGE_SIZE, PAGE_COUNT,
        CSdfatlibVirtMemAlloc<POOL_SIZE, PAGE_SIZE, PAGE_COUNT> >
{
    SdFat sd;
    SdFile sfile;

    void doStart(void)
    {
        if (!sd.begin(9, SPI_HALF_SPEED))
            sd.initErrorHalt();

        if (!sfile.open("ramfile", O_CREAT | O_RDWR))
            sd.errorHalt("opening ram file failed");
    }

    void doSuspend(void) { } // UNDONE
    void doStop(void)
    {
        sfile.close();
        sfile.remove();
    }
    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        sfile.seekSet(offset);
        sfile.read(data, size);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        sfile.seekSet(offset);
        sfile.write(data, size);
    }

public:
    CSdfatlibVirtMemAlloc(void) { }
    ~CSdfatlibVirtMemAlloc(void) { doStop(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TSdfatlibVirtPtr { typedef CVirtPtr<T, CSdfatlibVirtMemAlloc<> > type; };

#endif // SDFATLIB_ALLOC_H

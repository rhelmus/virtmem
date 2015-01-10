#ifndef SDFATLIB_ALLOC_H
#define SDFATLIB_ALLOC_H

#include "alloc.h"

#include <SdFat.h>

// UNDONE: make this dependent upon platform
struct SSdfatlibMemAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 32;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 64;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 1024 * 4;
    static const uint32_t poolSize = DEFAULT_POOLSIZE;
};

template <typename> class CSdfatlibVirtMemAlloc;

template <typename TProperties=SSdfatlibMemAllocProperties>
class CSdfatlibVirtMemAlloc : public CVirtMemAlloc<TProperties, CSdfatlibVirtMemAlloc<TProperties> >
{
    SdFat sd;
    SdFile sfile;
    uint8_t pin;

    void doStart(void)
    {
        if (!sd.begin(pin, SPI_FULL_SPEED))
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
        const uint32_t t = micros();
        sfile.seekSet(offset);
        sfile.read(data, size);
        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        const uint32_t t = micros();
        sfile.seekSet(offset);
        sfile.write(data, size);
        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    CSdfatlibVirtMemAlloc(uint8_t p) : pin(p) { }
    ~CSdfatlibVirtMemAlloc(void) { doStop(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TSdfatlibVirtPtr { typedef CVirtPtr<T, CSdfatlibVirtMemAlloc<> > type; };

#endif // SDFATLIB_ALLOC_H

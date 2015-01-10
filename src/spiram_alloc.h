#ifndef SPIRAM_ALLOC_H
#define SPIRAM_ALLOC_H

#include "alloc.h"

#include <SPI.h>
#include <SpiRAM.h>

// UNDONE: multiple and different chips. For now just 23LC1024
// UNDONE: settings below
struct SSPIRAMMemAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 32;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 64;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 512 * 4;
    static const uint32_t poolSize = 1024 * 128; // 128 kB
};

template <typename> class CSdfatlibVirtMemAlloc;

template <typename TProperties=SSPIRAMMemAllocProperties>
class CSPIRAMVirtMemAlloc : public CVirtMemAlloc<TProperties, CSPIRAMVirtMemAlloc<TProperties> >
{
    int SPIDiv;
    uint8_t pin;
    SpiRAM SPIRAM;

    void doStart(void)
    {
        // UNDONE: make sure that SPI stuff is started here, not in constructor of SpiRAM...
        // same idea for doSuspend()/doStop()
    }

    void doSuspend(void) { } // UNDONE
    void doStop(void)
    {
    }
    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        const uint32_t t = micros();
        SPIRAM.read_stream(offset, (char *)data, size);
        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        const uint32_t t = micros();
        SPIRAM.write_stream(offset, (char *)data, size);
        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    CSPIRAMVirtMemAlloc(int div, uint8_t p) : SPIRAM(SPIDiv, pin, CHIP_23LCV1024), SPIDiv(div), pin(p) { }
    ~CSPIRAMVirtMemAlloc(void) { doStop(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TSPIRAMVirtPtr { typedef CVirtPtr<T, CSPIRAMVirtMemAlloc<> > type; };

#endif // SPIRAM_ALLOC_H

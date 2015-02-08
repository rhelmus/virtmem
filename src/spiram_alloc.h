#ifndef SPIRAM_ALLOC_H
#define SPIRAM_ALLOC_H

#include <Arduino.h>
#include "alloc.h"

#include <serialram.h>

// UNDONE: multiple and different chips. For now just 23LC1024
// UNDONE: settings below
struct SSPIRAMMemAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 32;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 64;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 1024;
    static const uint32_t poolSize = 1024 * 128; // 128 kB
};

template <typename TProperties=SSPIRAMMemAllocProperties>
class CSPIRAMVirtMemAlloc : public CVirtMemAlloc<TProperties, CSPIRAMVirtMemAlloc<TProperties> >
{
    CSerialRam serialRAM;
    bool largeAddressing;
    uint8_t chipSelect;
    CSerialRam::ESPISpeed SPISpeed;

    void doStart(void)
    {
        serialRAM.begin(largeAddressing, chipSelect, SPISpeed);
    }

    void doSuspend(void) { } // UNDONE
    void doStop(void)
    {
        serialRAM.end();
    }

    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        const uint32_t t = micros();
        serialRAM.read((char *)data, offset, size);
        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        const uint32_t t = micros();
        serialRAM.write((const char *)data, offset, size);
        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    CSPIRAMVirtMemAlloc(bool la, uint8_t cs, CSerialRam::ESPISpeed s) : largeAddressing(la), chipSelect(cs), SPISpeed(s) { }
    ~CSPIRAMVirtMemAlloc(void) { doStop(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TSPIRAMVirtPtr { typedef CVirtPtr<T, CSPIRAMVirtMemAlloc<> > type; };

#endif // SPIRAM_ALLOC_H

#ifndef VIRTMEM_SPIRAM_ALLOC_H
#define VIRTMEM_SPIRAM_ALLOC_H

#include <Arduino.h>
#include "alloc.h"

#include <serialram.h>

struct SSPIRamConfig
{
    uint32_t size;
    uint8_t chipSelect;
    CSerialRam::ESPISpeed speed;
};

// UNDONE: multiple and different chips. For now just 23LC1024
// UNDONE: settings below
struct SSPIRAMMemAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 32;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 64;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 512;
    static const uint32_t poolSize = 1024 * 128; // 128 kB
    static SSPIRamConfig SPIChips[] = {
        { 1024 * 128, 8, CSerialRam::SPEED_FULL }
    };
};

template <typename TProperties=SSPIRAMMemAllocProperties>
class CSPIRAMVirtMemAlloc : public CVirtMemAlloc<TProperties, CSPIRAMVirtMemAlloc<TProperties> >
{
    bool largeAddressing;
    uint8_t chipSelect;
    CSerialRam::ESPISpeed SPISpeed;
    CSerialRam serialRAM;

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
//        const uint32_t t = micros();
        serialRAM.read((char *)data, offset, size);
//        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        const uint32_t t = micros();
        serialRAM.write((const char *)data, offset, size);
//        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    CSPIRAMVirtMemAlloc(bool la, uint8_t cs, CSerialRam::ESPISpeed s) : largeAddressing(la), chipSelect(cs), SPISpeed(s) { }
    CSPIRAMVirtMemAlloc(void) { }
    ~CSPIRAMVirtMemAlloc(void) { doStop(); }

    void setSettings(bool la, uint8_t cs, CSerialRam::ESPISpeed s)
    {
        largeAdressing = la; chipSelect = cs; SPISpeed = s;
    }
};

template <typename TProperties=SSPIRAMMemAllocProperties>
class CMultiSPIRAMVirtMemAlloc : public CVirtMemAlloc<TProperties, CMultiSPIRAMVirtMemAlloc<TProperties> >
{
    enum { CHIP_AMOUNT = sizeof(TProperties::SPIChips) / sizeof(SSPIRamConfig) };

    CSerialRam serialRAM[CHIP_AMOUNT];

    void doStart(void)
    {
        for (uint8_t i=0; i<CHIP_AMOUNT; ++i)
            serialRAM[i].begin(TProperties::SPIChips[i].size > (1024 * 64),
                               TProperties::SPIChips[i].chipSelect, TProperties::SPIChips[i].speed);
    }

    void doSuspend(void) { } // UNDONE
    void doStop(void)
    {
        for (uint8_t i=0; i<CHIP_AMOUNT; ++i)
            serialRAM[i].end();
    }

    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        const uint32_t t = micros();
        TVirtPointer startptr = 0;
        for (uint8_t i=0; i<CHIP_AMOUNT; ++i)
        {
            const TVirtPointer endptr = startptr + serialRAM[i].size;
            if (offset >= startptr && offset < endptr)
            {
                const TVirtPointer p = offset - startptr; // address relative in this chip
                const TVirtPtrSize sz = private_utils::min(size, serialRAM[i].size);
                serialRAM[i].read((char *)data, p, sz);

                if (sz == size)
                    break;

                size -= sz;
                data = (char *)data + sz;
                offset += sz;
            }
            startptr = endptr;
        }
//        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        const uint32_t t = micros();
        TVirtPointer startptr = 0;
        for (uint8_t i=0; i<CHIP_AMOUNT; ++i)
        {
            const TVirtPointer endptr = startptr + serialRAM[i].size;
            if (offset >= startptr && offset < endptr)
            {
                const TVirtPointer p = offset - startptr; // address relative in this chip
                const TVirtPtrSize sz = private_utils::min(size, serialRAM[i].size);
                serialRAM[i].write((const char *)data, p, sz);

                if (sz == size)
                    break;

                size -= sz;
                data = (char *)data + sz;
                offset += sz;
            }
            startptr = endptr;
        }
//        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    CMultiSPIRAMVirtMemAlloc(void) { }
    ~CMultiSPIRAMVirtMemAlloc(void) { doStop(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TSPIRAMVirtPtr { typedef CVirtPtr<T, CSPIRAMVirtMemAlloc<> > type; };

#endif // VIRTMEM_SPIRAM_ALLOC_H

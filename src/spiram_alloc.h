#ifndef VIRTMEM_SPIRAM_ALLOC_H
#define VIRTMEM_SPIRAM_ALLOC_H

/**
  * @file
  * @brief This file contains the SPI RAM virtual memory allocator
  */

#include <Arduino.h>
#include "alloc.h"

#include <serialram.h>

namespace virtmem {

template <typename TProperties=SDefaultAllocProperties>
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

    void doStop(void)
    {
        serialRAM.end();
    }

    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        const uint32_t t = micros();
        serialRAM.read((char *)data, offset, size);
//        Serial.print("read: "); Serial.print(offset); Serial.print("/"); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        const uint32_t t = micros();
        serialRAM.write((const char *)data, offset, size);
//        Serial.print("write: "); Serial.print(offset); Serial.print("/"); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    CSPIRAMVirtMemAlloc(TVirtPtrSize ps, bool la, uint8_t cs, CSerialRam::ESPISpeed s) :
        largeAddressing(la), chipSelect(cs), SPISpeed(s) { this->setPoolSize(ps); }
    CSPIRAMVirtMemAlloc(TVirtPtrSize ps) { this->setPoolSize(ps); }
    CSPIRAMVirtMemAlloc(void) { }
    ~CSPIRAMVirtMemAlloc(void) { doStop(); }

    void setSettings(bool la, uint8_t cs, CSerialRam::ESPISpeed s)
    {
        largeAddressing = la; chipSelect = cs; SPISpeed = s;
    }
};


struct SSPIRamConfig
{
    bool largeAddressing;
    uint32_t size;
    uint8_t chipSelect;
    CSerialRam::ESPISpeed speed;
};

template <const SSPIRamConfig *SPIChips, size_t chipAmount, typename TProperties=SDefaultAllocProperties>
class CMultiSPIRAMVirtMemAlloc : public CVirtMemAlloc<TProperties, CMultiSPIRAMVirtMemAlloc<SPIChips, chipAmount, TProperties> >
{
    CSerialRam serialRAM[chipAmount];

    void doStart(void)
    {
        for (uint8_t i=0; i<chipAmount; ++i)
            serialRAM[i].begin(SPIChips[i].largeAddressing, SPIChips[i].chipSelect, SPIChips[i].speed);
        Serial.print("poolsize: "); Serial.print(this->getPoolSize());
        Serial.print(", "); Serial.print(sizeof(SSPIRamConfig));
        Serial.print(", "); Serial.println(chipAmount);
    }

    void doStop(void)
    {
        for (uint8_t i=0; i<chipAmount; ++i)
            serialRAM[i].end();
    }

    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        const uint32_t t = micros();
        TVirtPointer startptr = 0;
        for (uint8_t i=0; i<chipAmount; ++i)
        {
            const TVirtPointer endptr = startptr + SPIChips[i].size;
            if (offset >= startptr && offset < endptr)
            {
                const TVirtPointer p = offset - startptr; // address relative in this chip
                const TVirtPtrSize sz = private_utils::minimal(size, SPIChips[i].size - p);
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
        for (uint8_t i=0; i<chipAmount; ++i)
        {
            const TVirtPointer endptr = startptr + SPIChips[i].size;
            if (offset >= startptr && offset < endptr)
            {
                const TVirtPointer p = offset - startptr; // address relative in this chip
                const TVirtPtrSize sz = private_utils::minimal(size, SPIChips[i].size - p);
                serialRAM[i].write((const char *)data, p, sz);

                if (sz == size)
                    break;

                size -= sz;
                data = (const char *)data + sz;
                offset += sz;
            }
            startptr = endptr;
        }
//        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    using CBaseVirtMemAlloc::setPoolSize;

public:
    CMultiSPIRAMVirtMemAlloc(void)
    {
        uint32_t ps = 0;
        for (uint8_t i=0; i<chipAmount; ++i)
            ps += SPIChips[i].size;
        this->setPoolSize(ps);
    }
    ~CMultiSPIRAMVirtMemAlloc(void) { doStop(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TSPIRAMVirtPtr { typedef CVirtPtr<T, CSPIRAMVirtMemAlloc<> > type; };

}

#endif // VIRTMEM_SPIRAM_ALLOC_H

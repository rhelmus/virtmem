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

/**
 * @brief Virtual allocator that uses SPI (serial) RAM (i.e. the 23LC series from Microchip)
 * as memory pool.
 *
 * This class uses an external SRAM chip as a memory pool. Interfacing occurs through the
 * [serram library](https://github.com/rhelmus/serialram) and must be installed in order to use
 * this allocator.
 *
 * @tparam Properties Allocator properties, see DefaultAllocProperties
 *
 * @note The `serram` library needs to be initialized (i.e. by calling CSerial::begin()) *before*
 * initializing this allocator.
 * @sa @ref bUsing and MultiSPIRAMVAlloc
 */
template <typename Properties=DefaultAllocProperties>
class SPIRAMAlloc : public VAlloc<Properties, SPIRAMAlloc<Properties> >
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

    void doRead(void *data, VPtrSize offset, VPtrSize size)
    {
//        const uint32_t t = micros();
        serialRAM.read((char *)data, offset, size);
//        Serial.print("read: "); Serial.print(offset); Serial.print("/"); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, VPtrSize offset, VPtrSize size)
    {
//        const uint32_t t = micros();
        serialRAM.write((const char *)data, offset, size);
//        Serial.print("write: "); Serial.print(offset); Serial.print("/"); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    /**
     * @brief Constructs (but not initializes) the allocator.
     * @param ps Total amount of bytes of the memory pool (i.e. the size of the SRAM chip)
     * @param la `true` if large addressing (chipsize >= 1mbit) should be used
     * @param cs Chip select (CS) pin connected to the SRAM chip
     * @param s SPI speed (CSerialRam::SPEED_FULL, CSerialRam::SPEED_HALF or
     * CSerialRam::SPEED_QUARTER)
     * @sa setSettings and setPoolSize
     */
    SPIRAMAlloc(VPtrSize ps, bool la, uint8_t cs, CSerialRam::ESPISpeed s) :
        largeAddressing(la), chipSelect(cs), SPISpeed(s) { this->setPoolSize(ps); }
    /**
     * @brief Constructs (but not initializes) the allocator.
     * @param ps Total amount of bytes of the memory pool (i.e. the size of the SRAM chip)
     * @sa setSettings and setPoolSize
     */
    SPIRAMAlloc(VPtrSize ps) { this->setPoolSize(ps); }
    SPIRAMAlloc(void) { } //!< Constructs (but not initializes) the allocator.
    ~SPIRAMAlloc(void) { doStop(); }

    /**
     * @brief Configures the allocator.
     *
     * See SPIRAMVAlloc::SPIRAMVAlloc for a description of the parameters.
     * @note This function should only be called if the allocator is not initialized.
     */
    void setSettings(bool la, uint8_t cs, CSerialRam::ESPISpeed s)
    {
        largeAddressing = la; chipSelect = cs; SPISpeed = s;
    }
};


/**
 * @brief This `struct` is used to configure each SRAM chip used by a MultiSPIRAMVAlloc
 * allocator.
 */
struct SPIRamConfig
{
    bool largeAddressing; //!< Does this chip needs large addressing (`true` if size >= 1 Mbit)
    uint32_t size; //!< Amount of bytes this chip can hold
    uint8_t chipSelect; //!< Pin assigned as chip select (CS) for this chip
    CSerialRam::ESPISpeed speed; //!< SPI speed to be used: CSerialRam::SPEED_FULL, CSerialRam::SPEED_HALF or CSerialRam::SPEED_QUARTER
};

/**
 * @brief Virtual allocator that uses multiple SRAM chips (i.e. 23LC series from Microchip)
 * as memory pool.
 *
 * This allocator is similar to SPIRAMVAlloc, but combines multiple SRAM chips as
 * one large memory pool. Interfacing occurs through the
 * [serram library](https://github.com/rhelmus/serialram) and must be installed in order to use
 * this allocator.
 *
 * @tparam SPIChips An array of SPIRamConfig that is used to configure each individual SRAM chip.
 * @tparam chipAmount Amount of SRAM chips to be used.
 * @tparam Properties Allocator properties, see DefaultAllocProperties
 *
 * @note The `serram` library needs to be initialized (i.e. by calling CSerial::begin()) *before*
 * initializing this allocator.
 * @sa @ref bUsing, SPIRamConfig and SPIRAMVAlloc
 *
 */
template <const SPIRamConfig *SPIChips, size_t chipAmount, typename Properties=DefaultAllocProperties>
class MultiSPIRAMVAlloc : public VAlloc<Properties, MultiSPIRAMVAlloc<SPIChips, chipAmount, Properties> >
{
    CSerialRam serialRAM[chipAmount];

    void doStart(void)
    {
        for (uint8_t i=0; i<chipAmount; ++i)
            serialRAM[i].begin(SPIChips[i].largeAddressing, SPIChips[i].chipSelect, SPIChips[i].speed);
        Serial.print("poolsize: "); Serial.print(this->getPoolSize());
        Serial.print(", "); Serial.print(sizeof(SPIRamConfig));
        Serial.print(", "); Serial.println(chipAmount);
    }

    void doStop(void)
    {
        for (uint8_t i=0; i<chipAmount; ++i)
            serialRAM[i].end();
    }

    void doRead(void *data, VPtrSize offset, VPtrSize size)
    {
//        const uint32_t t = micros();
        VPtrNum startptr = 0;
        for (uint8_t i=0; i<chipAmount; ++i)
        {
            const VPtrNum endptr = startptr + SPIChips[i].size;
            if (offset >= startptr && offset < endptr)
            {
                const VPtrNum p = offset - startptr; // address relative in this chip
                const VPtrSize sz = private_utils::minimal(size, SPIChips[i].size - p);
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

    void doWrite(const void *data, VPtrSize offset, VPtrSize size)
    {
//        const uint32_t t = micros();
        VPtrNum startptr = 0;
        for (uint8_t i=0; i<chipAmount; ++i)
        {
            const VPtrNum endptr = startptr + SPIChips[i].size;
            if (offset >= startptr && offset < endptr)
            {
                const VPtrNum p = offset - startptr; // address relative in this chip
                const VPtrSize sz = private_utils::minimal(size, SPIChips[i].size - p);
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

    using BaseVAlloc::setPoolSize;

public:
    /**
     * Constructs the allocator. The pool size is automatically
     * deduced from the chip configurations.
     */
    MultiSPIRAMVAlloc(void)
    {
        uint32_t ps = 0;
        for (uint8_t i=0; i<chipAmount; ++i)
            ps += SPIChips[i].size;
        this->setPoolSize(ps);
    }
    ~MultiSPIRAMVAlloc(void) { doStop(); }
};

template <typename, typename> class VPtr;
template <typename T> struct TSPIRAMVirtPtr { typedef VPtr<T, SPIRAMAlloc<> > type; };

}

#endif // VIRTMEM_SPIRAM_ALLOC_H

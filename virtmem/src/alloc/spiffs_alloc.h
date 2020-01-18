#ifndef VIRTMEM_SPIFFS_ALLOC_H
#define VIRTMEM_SPIFFS_ALLOC_H

#if defined (ESP8266) || defined (ESP32)
/**
  * @file
  * @brief This file contains the SPIFFS virtual memory allocator
  */

#include <SPI.h>
#include "internal/alloc.h"

#if defined (ESP8266)
  #include <FS.h>
#elif defined (ESP32)
  #include <SPIFFS.h>
#endif

namespace virtmem {

/**
 * @brief Virtual memory allocator class that uses ESP8266/ESP32 SPIFFS as virtual pool
 *
 * This class uses a file on an FAT formatted SPIFFS as virtual memory pool. 
 *
 * When the allocator is initialized (i.e. by calling start()) it will create a file called
 * 'ramfile.vm' in the root directory. Existing files will be reused and resized if necessary.
 *
 * @tparam Properties Allocator properties, see DefaultAllocProperties
 *
 * @note The SD FAT library needs to be initialized (i.e. by calling SPIFFS::begin()) *before*
 * initializing this allocator. The SPIFFS may also to be formatted by calling SPIFFS::format()).
 * @sa @ref bUsing, SPIFFSVAlloc
 */
template <typename Properties=DefaultAllocProperties>
class SPIFFSVAllocP : public VAlloc<Properties, SPIFFSVAllocP<Properties> >
{
    File spiffsFile;

    void doStart(void)
    {
        spiffsFile = SPIFFS.open("/ramfile.vm", "w+");
        if (!spiffsFile)
        {
            Serial.println("opening ram file failed");
            while (true)
                ;
        }

        const uint32_t size = spiffsFile.size();
        if (size < this->getPoolSize())
            this->writeZeros(size, this->getPoolSize() - size);
    }

    void doStop(void)
    {
        spiffsFile.close();
    }

    void doRead(void *data, VPtrSize offset, VPtrSize size)
    {
        //const uint32_t t = micros();
        spiffsFile.seek(offset, SeekSet);
        spiffsFile.readBytes((char *)data, size);
        //Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, VPtrSize offset, VPtrSize size)
    {
        //const uint32_t t = micros();
        spiffsFile.seek(offset, SeekSet);
        spiffsFile.write((const uint8_t *)data, size);
        //Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    /** Constructs (but not initializes) the SPIFFS allocator.
     * @param ps The size of the virtual memory pool
     * @sa setPoolSize
     */
    SPIFFSVAllocP(VPtrSize ps=VIRTMEM_DEFAULT_POOLSIZE) { 
        this->setPoolSize(ps);
    }
    ~SPIFFSVAllocP(void) { 
        doStop();
    }

    /**
     * Removes the temporary file used as virtual memory pool.
     * @note Only call this when the allocator is not initialized!
     */
    void removeTempFile(void) { SPIFFS.remove(spiffsFile.name()); }
};

typedef SPIFFSVAllocP<> SPIFFSVAlloc; //!< Shortcut to SPIFFSVAllocP with default template arguments

/**
 * @example spiffs_simple.ino
 * This is a simple example sketch showing how to use the SPIFFS allocator.
 */

}

#endif
#endif // VIRTMEM_SPIFFS_ALLOC_H

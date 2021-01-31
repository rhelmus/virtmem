#ifndef VIRTMEM_LittleFS_ALLOC_H
#define VIRTMEM_LittleFS_ALLOC_H

/**
  * @file
  * @brief This file contains the LittleFS virtual memory allocator
  */

#if !defined(ESP32) && !defined(ESP8266)
#error "LittleFS is only supported on ESP32 or ESP8266"
#endif

#include "internal/alloc.h"

#include <LittleFS.h>

namespace virtmem {

#define RAMFILE F("/ramfile.vm")

/**
 * @brief Virtual memory allocator class that uses flash memory as virtual pool
 *
 * This class uses a file on LittleFS formatted flash as virtual memory pool.
 *
 * When the allocator is initialized (i.e. by calling start()) it will create a file called
 * 'ramfile.vm' in the root directory. Existing files will be reused and resized if necessary.
 *
 * @tparam Properties Allocator properties, see DefaultAllocProperties
 *
 * @sa @ref bUsing, SDVAlloc
 */
template <typename Properties=DefaultAllocProperties>
class LittleFSVAllocP : public VAlloc<Properties, LittleFSVAllocP<Properties> >
{
    File fileHandle;

    void doStart(void)
    {
      // If info returns false the FS is not mounted
      FSInfo fs_info;
      if (!LittleFS.info(fs_info)) {
        if (!LittleFS.begin()) {
          Serial.println("LittleFS mount failed");
          while (true);
        }
      }

      // open the file for writing, creating if it does not exist
      fileHandle = LittleFS.open(RAMFILE, "w+");
      if (!fileHandle)
      {
          Serial.printf("opening ram file (%s) failed\n", RAMFILE);
          while (true);
      }

      // resize the file if required
      const uint32_t size = fileHandle.size();
      if (size < this->getPoolSize())
          this->writeZeros(size, this->getPoolSize() - size);
    }

    void doStop(void)
    {
        fileHandle.close();
    }
    void doRead(void *data, VPtrSize offset, VPtrSize size)
    {
//        const uint32_t t = micros();
        fileHandle.seek(offset, SeekSet);
        fileHandle.read((uint8_t*) data, size);
//        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, VPtrSize offset, VPtrSize size)
    {
//        const uint32_t t = micros();
        fileHandle.seek(offset, SeekSet);
        fileHandle.write((const uint8_t*) data, size);
        fileHandle.flush();
//        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    /** Constructs (but not initializes) the SD FAT allocator.
     * @param ps The size of the virtual memory pool
     * @sa setPoolSize
     */
    LittleFSVAllocP(VPtrSize ps=VIRTMEM_DEFAULT_POOLSIZE) { this->setPoolSize(ps); }
    ~LittleFSVAllocP(void) { doStop(); }

    /**
     * Removes the temporary file used as virtual memory pool.
     * @note Only call this when the allocator is not initialized!
     */
    void removeTempFile(void) { LittleFS.remove(RAMFILE); }
};

typedef LittleFSVAllocP<> LittleFSVAlloc; //!< Shortcut to SDVAllocP with default template arguments

/**
 * @example sd_simple.ino
 * This is a simple example sketch showing how to use the LittleFS allocator.
 */

}

#endif // VIRTMEM_LittleFS_ALLOC_H

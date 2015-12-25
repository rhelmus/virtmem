#ifndef VIRTMEM_SD_ALLOC_H
#define VIRTMEM_SD_ALLOC_H

/**
  * @file
  * @brief This file contains the SD fat virtual memory allocator
  */

#include "internal/alloc.h"

#include <SdFat.h>

namespace virtmem {

/**
 * @brief Virtual memory allocator class that uses SD card as virtual pool
 *
 * This class uses a file on an FAT formatted SD card as virtual memory pool. The
 * [SD FAT library](https://github.com/greiman/SdFat) is used to interface with the SD card
 * and therefore has to be installed.
 *
 * When the allocator is initialized (i.e. by calling start()) it will create a file called
 * 'ramfile.vm' in the root directory. Existing files will be reused and resized if necessary.
 *
 * @tparam Properties Allocator properties, see DefaultAllocProperties
 *
 * @note The SD FAT library needs to be initialized (i.e. by calling SdFat::begin()) *before*
 * initializing this allocator.
 * @sa @ref bUsing, SDVAlloc
 */
template <typename Properties=DefaultAllocProperties>
class SDVAllocP : public VAlloc<Properties, SDVAllocP<Properties> >
{
    SdFile sdFile;

    void doStart(void)
    {
        // file does not exist yet (can we create it)?
        if (sdFile.open("ramfile.vm", O_CREAT | O_EXCL))
            this->writeZeros(0, this->getPoolSize()); // make it the right size
        else // already exists, check size
        {
            if (!sdFile.open("ramfile.vm", O_CREAT | O_RDWR))
            {
                Serial.println("opening ram file failed");
                while (true)
                    ;
            }

            const uint32_t size = sdFile.fileSize();
            if (size < this->getPoolSize())
                this->writeZeros(size, this->getPoolSize() - size);
        }
    }

    void doStop(void)
    {
        sdFile.close();
    }
    void doRead(void *data, VPtrSize offset, VPtrSize size)
    {
//        const uint32_t t = micros();
        sdFile.seekSet(offset);
        sdFile.read(data, size);
//        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, VPtrSize offset, VPtrSize size)
    {
//        const uint32_t t = micros();
        sdFile.seekSet(offset);
        sdFile.write(data, size);
//        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    /** Constructs (but not initializes) the SD FAT allocator.
     * @param ps The size of the virtual memory pool
     * @sa setPoolSize
     */
    SDVAllocP(VPtrSize ps=VIRTMEM_DEFAULT_POOLSIZE) { this->setPoolSize(ps); }
    ~SDVAllocP(void) { doStop(); }

    /**
     * Removes the temporary file used as virtual memory pool.
     * @note Only call this when the allocator is not initialized!
     */
    void removeTempFile(void) { sdFile.remove(); }
};

typedef SDVAllocP<> SDVAlloc; //!< Shortcut to SDVAllocP with default template arguments

/**
 * @example sd_simple.ino
 * This is a simple example sketch showing how to use the SD allocator.
 */

}

#endif // VIRTMEM_SD_ALLOC_H

#ifndef VIRTMEM_SD_ALLOC_H
#define VIRTMEM_SD_ALLOC_H

/**
  * @file
  * @brief This file contains the SD fat virtual memory allocator
  */

#include "alloc.h"

#include <SdFat.h>

namespace virtmem {

template <typename TProperties> class CSDVirtMemAlloc;

/**
 * @brief Virtual allocator class that uses SD card as virtual pool
 *
 * This class uses a file on an FAT formatted SD card as virtual memory pool. The
 * [SD FAT library](https://github.com/greiman/SdFat) is used to interface with the SD card.
 *
 * When the allocator is initialized (i.e. by calling start()) it will create a file called
 * 'ramfile.vm' in the root directory. Existing files will be reused and resized if necessary.
 *
 * @tparam TProperties Allocator properties, see SDefaultAllocProperties
 *
 * @note The SD FAT library needs to be initialized (i.e. by calling SdFat::begin()) *before*
 * initializing this allocator.
 * @sa @ref bUsing
 */
template <typename TProperties=SDefaultAllocProperties>
class CSDVirtMemAlloc : public CVirtMemAlloc<TProperties, CSDVirtMemAlloc<TProperties> >
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
    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        const uint32_t t = micros();
        sdFile.seekSet(offset);
        sdFile.read(data, size);
//        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        const uint32_t t = micros();
        sdFile.seekSet(offset);
        sdFile.write(data, size);
//        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    /** Constructs (but not initializes) the SD FAT allocator.
     * @param ps The size of the virtual memory pool
     */
    CSDVirtMemAlloc(TVirtPtrSize ps=DEFAULT_POOLSIZE) { this->setPoolSize(ps); }
    ~CSDVirtMemAlloc(void) { doStop(); }

    /**
     * Removes the temporary file used as virtual memory pool.
     * @note Only call this when the allocator is not initialized!
     */
    void removeTempFile(void) { sdFile.remove(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TSDVirtPtr { typedef CVirtPtr<T, CSDVirtMemAlloc<> > type; };

}

#endif // VIRTMEM_SD_ALLOC_H

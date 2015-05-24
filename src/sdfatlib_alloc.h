#ifndef VIRTMEM_SDFATLIB_ALLOC_H
#define VIRTMEM_SDFATLIB_ALLOC_H

#include "alloc.h"

#include <SdFat.h>

namespace virtmem {

template <typename> class CSdfatlibVirtMemAlloc;

template <typename TProperties=SDefaultAllocProperties>
class CSdfatlibVirtMemAlloc : public CVirtMemAlloc<TProperties, CSdfatlibVirtMemAlloc<TProperties> >
{
    SdFile sdFile;

    void doStart(void)
    {
        // file does not exist yet (can we create it)?
        if (sdFile.open("ramfile.vir", O_CREAT | O_EXCL))
            this->writeZeros(0, this->getPoolSize()); // make it the right size
        else // already exists, check size
        {
            if (!sdFile.open("ramfile", O_CREAT | O_RDWR))
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
        sdFile.remove();
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
    CSdfatlibVirtMemAlloc(TVirtPtrSize ps=DEFAULT_POOLSIZE) { this->setPoolSize(ps); }
    ~CSdfatlibVirtMemAlloc(void) { doStop(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TSdfatlibVirtPtr { typedef CVirtPtr<T, CSdfatlibVirtMemAlloc<> > type; };

}

#endif // VIRTMEM_SDFATLIB_ALLOC_H

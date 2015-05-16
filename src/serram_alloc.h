#ifndef VIRTMEM_SERRAM_ALLOC_H
#define VIRTMEM_SERRAM_ALLOC_H

#include <Arduino.h>
#include "alloc.h"
#include "serram_utils.h"

template <typename IOStream=typeof(Serial), typename TProperties=SDefaultAllocProperties>
class CSerRAMVirtMemAlloc : public CVirtMemAlloc<TProperties, CSerRAMVirtMemAlloc<IOStream, TProperties> >
{
    uint32_t baudRate;
    IOStream *stream;

    void doStart(void)
    {
        SerramUtils::init(stream, baudRate, this->getPoolSize());
    }

    void doSuspend(void) { } // UNDONE
    void doStop(void) { }

    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        uint32_t t = micros();
        SerramUtils::sendReadCommand(stream, SerramUtils::CMD_READ);
        SerramUtils::writeUInt32(stream, offset);
        SerramUtils::writeUInt32(stream, size);
        Serial.flush();
        SerramUtils::readBlock(stream, (char *)data, size);
//        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        const uint32_t t = micros();
        SerramUtils::sendWriteCommand(stream, SerramUtils::CMD_WRITE);
        SerramUtils::writeUInt32(stream, offset);
        SerramUtils::writeUInt32(stream, size);
        SerramUtils::writeBlock(stream, (const uint8_t *)data, size);
//        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    SerramUtils::CSerialInput<IOStream> input;

    CSerRAMVirtMemAlloc(TVirtPtrSize ps=DEFAULT_POOLSIZE, uint32_t baud=115200, IOStream *s=&Serial) :
        baudRate(baud), stream(s), input(stream) { this->setPoolSize(ps); }

    // only works before start() is called
    void setBaudRate(uint32_t baud) { baudRate = baud; }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TSerRAMVirtPtr { typedef CVirtPtr<T, CSerRAMVirtMemAlloc<typeof(Serial)> > type; };


#endif // VIRTMEM_SERRAM_ALLOC_H

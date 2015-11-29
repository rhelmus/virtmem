#ifndef VIRTMEM_SERIAL_ALLOC_H
#define VIRTMEM_SERIAL_ALLOC_H

/**
  * @file
  * @brief This file contains the serial virtual memory allocator
  */

#include <Arduino.h>
#include "alloc.h"
#include "serial_utils.h"

namespace virtmem {

template <typename IOStream=typeof(Serial), typename Properties=DefaultAllocProperties>
class SerialVAlloc : public VAlloc<Properties, SerialVAlloc<IOStream, Properties> >
{
    uint32_t baudRate;
    IOStream *stream;

    void doStart(void)
    {
        serram_utils::init(stream, baudRate, this->getPoolSize());
    }

    void doStop(void) { }

    void doRead(void *data, VPtrSize offset, VPtrSize size)
    {
//        uint32_t t = micros();
        serram_utils::sendReadCommand(stream, serram_utils::CMD_READ);
        serram_utils::writeUInt32(stream, offset);
        serram_utils::writeUInt32(stream, size);
        Serial.flush();
        serram_utils::readBlock(stream, (char *)data, size);
//        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, VPtrSize offset, VPtrSize size)
    {
//        const uint32_t t = micros();
        serram_utils::sendWriteCommand(stream, serram_utils::CMD_WRITE);
        serram_utils::writeUInt32(stream, offset);
        serram_utils::writeUInt32(stream, size);
        serram_utils::writeBlock(stream, (const uint8_t *)data, size);
//        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    serram_utils::SerialInput<IOStream> input;

    SerialVAlloc(VPtrSize ps=DEFAULT_POOLSIZE, uint32_t baud=115200, IOStream *s=&Serial) :
        baudRate(baud), stream(s), input(stream) { this->setPoolSize(ps); }

    // only works before start() is called
    void setBaudRate(uint32_t baud) { baudRate = baud; }

    uint32_t ping(void) const
    {
        serram_utils::sendReadCommand(stream, serram_utils::CMD_PING);
        const uint32_t starttime = micros();
        while (!serram_utils::waitForCommand(stream, serram_utils::CMD_PING, 1000))
            ;
        return micros() - starttime;
    }
};

template <typename, typename> class VPtr;
template <typename T> struct TSerRAMVirtPtr { typedef VPtr<T, SerialVAlloc<typeof(Serial)> > type; };

}

#endif // VIRTMEM_SERIAL_ALLOC_H

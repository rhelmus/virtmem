#ifndef SERRAM_ALLOC_H
#define SERRAM_ALLOC_H

#include <Arduino.h>
#include "alloc.h"
#include "serram_utils.h"

// UNDONE: settings below
struct SSerRAMMemAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 32;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 64;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 512;
    static const uint32_t poolSize = 1024 * 1024; // 1024 kB
};

template <typename TProperties=SSerRAMMemAllocProperties>
class CSerRAMVirtMemAlloc : public CVirtMemAlloc<TProperties, CSerRAMVirtMemAlloc<TProperties> >
{
    uint32_t baudRate;

    void doStart(void)
    {
        SerramUtils::init(baudRate, TProperties::poolSize);
    }

    void doSuspend(void) { } // UNDONE
    void doStop(void) { }

    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        const uint32_t t = micros();
        SerramUtils::sendReadCommand(SerramUtils::CMD_READ);
        SerramUtils::writeUInt32(offset);
        SerramUtils::writeUInt32(size);
        SerramUtils::waitForCommand(SerramUtils::CMD_READ);
        SerramUtils::readBlock((char *)data, size);
//        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
//        const uint32_t t = micros();
        SerramUtils::sendWriteCommand(SerramUtils::CMD_WRITE);
        SerramUtils::writeUInt32(offset);
        SerramUtils::writeUInt32(size);
        SerramUtils::writeBlock((const char *)data, size);
//        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    CSerRAMVirtMemAlloc(uint32_t baud=115200) : baudRate(baud) { }

    // only works before start() is called
    void setBaudRate(uint32_t baud) { baudRate = baud; }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TSerRAMVirtPtr { typedef CVirtPtr<T, CSerRAMVirtMemAlloc<> > type; };


#endif // SERRAM_ALLOC_H

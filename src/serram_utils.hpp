#include <Arduino.h>

#include "serram_utils.h"

namespace virtmem {

namespace serram_utils {

enum { CMD_START = 0xFF };

template <typename IOStream> void purgeSerial(IOStream *stream)
{
    uint32_t n;
    while ((n = stream->available()))
    {
        for (; n; --n)
            stream->read();
    }
}

template <typename IOStream> void writeUInt32(IOStream *stream, uint32_t i)
{
    stream->write(i & 0xFF);
    stream->write((i >> 8) & 0xFF);
    stream->write((i >> 16) & 0xFF);
    stream->write((i >> 24) & 0xFF);
}

template <typename IOStream> void writeBlock(IOStream *stream, const uint8_t *data, uint32_t size)
{
    stream->write(data, size);
}

template <typename IOStream> uint32_t readUInt32(IOStream *stream)
{
    uint8_t i = 0;
    uint32_t ret = 0;
    while (i < 4)
    {
        if (stream->available())
        {
            ret |= (stream->read() << (i * 8));
            ++i;
        }
    }
    return ret;
}

template <typename IOStream> uint16_t readUInt16(IOStream *stream)
{
    while (true)
    {
        if (stream->available() >= 2)
            return stream->read() | (stream->read() << 8);
    }
}

template <typename IOStream> uint8_t readUInt8(IOStream *stream)
{
    while (!stream->available())
        ;
    return stream->read();
}

template <typename IOStream> void readBlock(IOStream *stream, char *data, uint32_t size)
{
    while (size)
        size -= stream->readBytes(data, size);
}

template <typename IOStream> void sendWriteCommand(IOStream *stream, uint8_t cmd)
{
    stream->write(CMD_START);
    stream->write(cmd);
}

template <typename IOStream> void sendReadCommand(IOStream *stream, uint8_t cmd)
{
    purgeSerial(stream);
    stream->write(CMD_START);
    stream->write(cmd);
}

template <typename IOStream> bool waitForCommand(IOStream *stream, uint8_t cmd, uint8_t timeout)
{
    stream->flush();
    const uint32_t endtime = millis() + timeout;

    bool gotinit = false;
    while (millis() < endtime)
    {
        if (Serial.available())
        {
            const uint8_t b = stream->read();

            if (!gotinit && b == CMD_START)
                gotinit = true;
            else if (gotinit && b == cmd)
                return true;
        }
    }

    return false;
}

template <typename IOStream> void init(IOStream *stream, uint32_t baud, uint32_t poolsize)
{
    stream->begin(baud);

    // handshake
    while (true)
    {
        sendWriteCommand(stream, CMD_INIT);
        if (waitForCommand(stream, CMD_INIT, 50))
            break;
    }
#if 0
    waitForCommand(stream, CMD_INIT);
    sendWriteCommand(stream, CMD_INIT); // reply
#endif

    sendWriteCommand(stream, CMD_INITPOOL);
    writeUInt32(stream, poolsize);
}


template <typename IOStream> uint32_t SerialInput<IOStream>::available()
{
    sendReadCommand(stream, CMD_INPUTAVAILABLE);
    stream->flush();
    return readUInt32(stream);
}

template <typename IOStream> uint32_t SerialInput<IOStream>::availableAtLeast()
{
    if (availableMin == 0)
        availableMin = available();
    return availableMin;
}

template <typename IOStream> int16_t SerialInput<IOStream>::read()
{
    sendReadCommand(stream, CMD_INPUTREQUEST);
    writeUInt32(stream, 1);
    stream->flush();

    if (readUInt32(stream) == 0)
        return -1; // no data

    if (availableMin)
        --availableMin;
    return readUInt8(stream);
}

template <typename IOStream> uint32_t SerialInput<IOStream>::readBytes(char *buffer, uint32_t count)
{
    sendReadCommand(stream, CMD_INPUTREQUEST);
    writeUInt32(stream, count);
    stream->flush();
    count = readUInt32(stream);
    for (; count; --count, ++buffer)
        *buffer = readUInt8(stream);
    if (availableMin > count)
        availableMin -= count;
    else
        availableMin = 0;
    return count;
}

template <typename IOStream> int16_t SerialInput<IOStream>::peek()
{
    sendReadCommand(stream, CMD_INPUTPEEK);
    stream->flush();
    if (readUInt8(stream) == 0)
        return -1; // nothing there
    return readUInt8(stream);
}


}

}

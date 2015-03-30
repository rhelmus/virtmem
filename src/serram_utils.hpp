#include <Arduino.h>

#include "serram_utils.h"

namespace SerramUtils {

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
    uint32_t time = micros();
    while (!stream->available())
        ;
    time = micros() - time;
    stream->print("readUInt8: "); stream->println(time);
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

template <typename IOStream> void waitForCommand(IOStream *stream, uint8_t cmd)
{
    stream->flush();
    while (readUInt8(stream) != CMD_START)
        ;

    bool gotinit = false;
    while (true)
    {
        const uint8_t b = readUInt8(stream);
        if (!gotinit && b == CMD_START)
            gotinit = true;
        else if (gotinit && b == cmd)
            break;
    }
}

template <typename IOStream> void init(IOStream *stream, uint32_t baud, uint32_t poolsize)
{
    stream->begin(baud);

    // handshake
    waitForCommand(stream, CMD_INIT);
    sendWriteCommand(stream, CMD_INIT); // reply

    sendWriteCommand(stream, CMD_INITPOOL);
    writeUInt32(stream, poolsize);
}


template <typename IOStream> uint32_t CSerialInput<IOStream>::available()
{
    sendReadCommand(stream, CMD_INPUTAVAILABLE);
    stream->flush();
    return readUInt32(stream);
}

template <typename IOStream> uint32_t CSerialInput<IOStream>::availableAtLeast()
{
    if (availableMin == 0)
        availableMin = available();
    return availableMin;
}

template <typename IOStream> int16_t CSerialInput<IOStream>::read()
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

template <typename IOStream> uint32_t CSerialInput<IOStream>::readBytes(char *buffer, uint32_t count)
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

template <typename IOStream> int16_t CSerialInput<IOStream>::peek()
{
    sendReadCommand(stream, CMD_INPUTPEEK);
    stream->flush();
    if (readUInt8(stream) == 0)
        return -1; // nothing there
    return readUInt8(stream);
}


}

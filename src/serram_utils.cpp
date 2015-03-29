#include <Arduino.h>

#include "serram_utils.h"

namespace {

enum { CMD_START = 0xFF };

void purgeSerial(void)
{
    uint32_t n;
    while (n = Serial.available())
    {
        for (; n; --n)
            Serial.read();
    }
}


}

namespace SerramUtils {

void writeUInt32(uint32_t i)
{
    Serial.write(i & 0xFF);
    Serial.write((i >> 8) & 0xFF);
    Serial.write((i >> 16) & 0xFF);
    Serial.write((i >> 24) & 0xFF);
}

void writeBlock(const char *data, uint32_t size)
{
    Serial.write(data, size);
}

uint32_t readUInt32()
{
    uint8_t i = 0;
    uint32_t ret = 0;
    while (i < 4)
    {
        if (Serial.available())
        {
            ret |= (Serial.read() << (i * 8));
            ++i;
        }
    }
    return ret;
}

uint16_t readUInt16()
{
    while (true)
    {
        if (Serial.available() >= 2)
            return Serial.read() | (Serial.read() << 8);
    }
}

uint8_t readUInt8()
{
    uint32_t time = micros();
    while (!Serial.available())
        ;
    time = micros() - time;
    Serial.print("readUInt8: "); Serial.println(time);
    return Serial.read();
}

void readBlock(char *data, uint32_t size)
{
    while (size)
        size -= Serial.readBytes(data, size);
}

void sendWriteCommand(uint8_t cmd)
{
    Serial.write(CMD_START);
    Serial.write(cmd);
}

void sendReadCommand(uint8_t cmd)
{
    purgeSerial();
    Serial.write(CMD_START);
    Serial.write(cmd);
}

void waitForCommand(uint8_t cmd)
{
    Serial.flush();
    /*while (readUInt8() != CMD_START)
        ;*/
#if 0
    bool gotinit = false;
    while (true)
    {
        const uint8_t b = readUInt8();
        if (!gotinit && b == CMD_START)
            gotinit = true;
        else if (gotinit && b == cmd)
            break;
    }
#endif
}

void init(uint32_t baud, uint32_t poolsize)
{
    Serial.begin(baud);

    // handshake
    waitForCommand(CMD_INIT);
    sendWriteCommand(CMD_INIT); // reply
#if 0
    bool gotcmd = false;
    while (true)
    {
        if (Serial.available())
        {
            const uint8_t ch = Serial.read();
            if (!gotcmd && ch == CMD_START)
                gotcmd = true;
            else if (gotcmd && ch == CMD_INIT)
            {
                sendWriteCommand(CMD_INIT); // reply
                break;
            }
        }
    }
#endif

    sendWriteCommand(CMD_INITPOOL);
    writeUInt32(poolsize);
}


uint32_t CSerialInput::available()
{
    sendReadCommand(CMD_INPUTAVAILABLE);
    waitForCommand(CMD_INPUTAVAILABLE);
    return readUInt32();
}

uint32_t CSerialInput::availableAtLeast()
{
    if (availableMin == 0)
        availableMin = available();
    return availableMin;
}

int16_t CSerialInput::read()
{
    sendReadCommand(CMD_INPUTREQUEST);
    writeUInt32(1);
    waitForCommand(CMD_INPUTREQUEST);

    if (readUInt32() == 0)
        return -1; // no data

    if (availableMin)
        --availableMin;
    return readUInt8();
}

uint32_t CSerialInput::readBytes(char *buffer, uint32_t count)
{
    sendReadCommand(CMD_INPUTREQUEST);
    writeUInt32(count);
    waitForCommand(CMD_INPUTREQUEST);
    count = readUInt32();
    for (; count; --count, ++buffer)
        *buffer = readUInt8();
    if (availableMin > count)
        availableMin -= count;
    else
        availableMin = 0;
    return count;
}

int16_t CSerialInput::peek()
{
    sendReadCommand(CMD_INPUTPEEK);
    waitForCommand(CMD_INPUTPEEK);
    if (readUInt8() == 0)
        return -1; // nothing there
    return readUInt8();
}


}

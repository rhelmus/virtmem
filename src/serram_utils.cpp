#include <Arduino.h>

#include "serram_utils.h"

namespace {

enum { CMD_START = 0xFF };

void purgeSerial(void)
{
    while (Serial.available())
        Serial.read();
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

uint8_t readUInt8()
{
    while (!Serial.available())
        ;
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
    bool gotinit = false;
    while (true)
    {
        const uint8_t b = readUInt8();
        if (!gotinit && b == CMD_START)
            gotinit = true;
        else if (gotinit && b == cmd)
            break;
    }
}

void init(uint32_t baud, uint32_t poolsize)
{
    Serial.begin(baud);

    // handshake
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

    sendWriteCommand(CMD_INITPOOL);
    writeUInt32(poolsize);
}

}

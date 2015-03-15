#ifndef VIRTMEM_SERRAM_UTILS_H
#define VIRTMEM_SERRAM_UTILS_H

namespace SerramUtils {

enum
{
    CMD_INIT = 0,
    CMD_INITPOOL,
    CMD_READ,
    CMD_WRITE,
    CMD_INPUTAVAILABLE,
    CMD_INPUTREQUEST,
    CMD_INPUTPEEK
};

void writeUInt32(uint32_t i);
void writeBlock(const char *data, uint32_t size);
uint32_t readUInt32(void);
uint16_t readUInt16(void);
uint8_t readUInt8(void);
void readBlock(char *data, uint32_t size);
void sendWriteCommand(uint8_t cmd);
void sendReadCommand(uint8_t cmd);
void waitForCommand(uint8_t cmd);
void init(uint32_t baud, uint32_t poolsize);

class CSerialInput
{
    uint32_t availableMin;

public:
    CSerialInput(void) : availableMin(0) { }

    uint32_t available(void);
    uint32_t availableAtLeast(void);
    int16_t read(void);
    uint32_t readBytes(char *buffer, uint32_t count);
    int16_t peek(void);
};

}

#endif // VIRTMEM_SERRAM_UTILS_H

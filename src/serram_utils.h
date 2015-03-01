#ifndef SERRAM_UTILS_H
#define SERRAM_UTILS_H

namespace SerramUtils {

enum { CMD_INIT = 0, CMD_INITPOOL, CMD_READ, CMD_WRITE };

void writeUInt32(uint32_t i);
void writeBlock(const char *data, uint32_t size);
uint32_t readUInt32(void);
uint8_t readUInt8(void);
void readBlock(char *data, uint32_t size);
void sendWriteCommand(uint8_t cmd);
void sendReadCommand(uint8_t cmd);
void waitForCommand(uint8_t cmd);
void init(uint32_t baud, uint32_t poolsize);

}

#endif // SERRAM_UTILS_H

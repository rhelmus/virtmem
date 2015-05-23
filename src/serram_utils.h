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
    CMD_INPUTPEEK,
    CMD_PING
};

template <typename IOStream> class CSerialInput
{
    uint32_t availableMin;
    IOStream *stream;

public:
    CSerialInput(IOStream *s) : availableMin(0), stream(s) { }

    uint32_t available(void);
    uint32_t availableAtLeast(void);
    int16_t read(void);
    uint32_t readBytes(char *buffer, uint32_t count);
    int16_t peek(void);
};

}

#include "serram_utils.hpp"

#endif // VIRTMEM_SERRAM_UTILS_H

#ifndef VIRTMEM_SERIAL_UTILS_H
#define VIRTMEM_SERIAL_UTILS_H

namespace virtmem {

namespace serram_utils {

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

template <typename IOStream> class SerialInput
{
    uint32_t availableMin;
    IOStream *stream;

public:
    SerialInput(IOStream *s) : availableMin(0), stream(s) { }

    uint32_t available(void);
    uint32_t availableAtLeast(void);
    int16_t read(void);
    uint32_t readBytes(char *buffer, uint32_t count);
    int16_t peek(void);
};

}

}

#include "serial_utils.hpp"

#endif // VIRTMEM_SERIAL_UTILS_H

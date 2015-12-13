#ifndef VIRTMEM_SERIAL_UTILS_H
#define VIRTMEM_SERIAL_UTILS_H

/**
  * @file
  * @brief This file contains utilities for the serial virtual memory allocator
  */

namespace virtmem {

namespace serram_utils {

//! @cond HIDDEN_SYMBOLS
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
//! @endcond

/**
 * @brief Utility class that handles serial input over a port that is used by by SerialVAlloc
 */
template <typename IOStream> class SerialInput
{
    uint32_t availableMin;
    IOStream *stream;

public:
    SerialInput(IOStream *s) : availableMin(0), stream(s) { }

    /**
     * @brief Available bytes that can be read via serial
     * @return Number of bytes that can be read
     */
    uint32_t available(void);

    /**
     * @brief Returns the minimum number of bytes that can be read.
     * @return The number of bytes that are minimally available to read.
     * @note This function tries to use cached data and is therefore often more efficient compared to available().
     */
    uint32_t availableAtLeast(void);

    /**
     * @brief Read a byte from serial input.
     * @return Returns the byte that was read, or -1 if none were available.
     */
    int16_t read(void);

    /**
     * @brief Read multiple bytes from serial input
     * @param buffer Destination character array to read data into.
     * @param count Amount of bytes to read
     * @return Amount of bytes to read
     */
    uint32_t readBytes(char *buffer, uint32_t count);

    /**
     * @brief Reads a byte from serial input without removing it from the serial buffer
     * @return Available byte to read, or -1 if none.
     */
    int16_t peek(void);
};

}

}

#include "serial_utils.hpp"

#endif // VIRTMEM_SERIAL_UTILS_H

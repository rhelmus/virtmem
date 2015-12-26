#ifndef VIRTMEM_SERIAL_ALLOC_H
#define VIRTMEM_SERIAL_ALLOC_H

/**
  * @file
  * @brief This file contains the serial virtual memory allocator
  */

#include <Arduino.h>
#include "internal/alloc.h"
#include "internal/serial_utils.h"

namespace virtmem {

/**
 * @brief Virtual memory allocator that uses external memory via a serial connection as
 * memory pool.
 *
 * This class utilizes RAM from an external device (e.g. a PC or ARM board) that is connected
 * with a serial port. This allocator is particularly useful when a permanant serial connection
 * with such a device exists, for instance on an Arduino Yun or Udoo ARM board. Since this
 * allocator requires no extra hardware (besides a serial connection), it is easy to test
 * and a good starting point and generally useful for debugging. The device that provides
 * the RAM (the _RAM host_) should run a script (`serial_host.py`) that is responsible for
 * communicating with `virtmem` and memory management.
 *
 * __Choosing a serial port__
 *
 * By default, the allocator uses the default serial port (i.e. the `Serial` class). In this case,
 * the @ref SerialVAlloc shortcut can also be used:
 * @code{.cpp}
 * SerialVAllocP<> valloc;
 * SerialVAlloc valloc2; // same as above, both use the default serial port (Serial)
 * @endcode
 * If another serial port is desired, both the _type_ and (a pointer to the) _variable_ must
 * be specified:
 * @code{.cpp}
 * // Uses Serial1 with a default pool size and baudrate
 * SerialVAllocP<typeof(Serial1)> valloc(VIRTMEM_DEFAULT_POOLSIZE, 115200, &Serial1);
 * @endcode
 * The `typeof` expression used in the above example may raise some questions. We need to know
 * the type of the serial class to be used (in this case the class that defines Serial1). Since
 * this not well standardized, we simply use the `typeof` expression to retrieve it. Note that this
 * is an extension of `gcc` (the compiler used by Arduino and many other platforms), and that
 * `decltype` could also be used on C++11 systems. Note that this approach is quite generic,
 * and as long as a class defines similar functions as the `SerialX` classes, other types
 * such as `SoftwareSerial` can also be used:
 * @code{.cpp}
 * SoftwareSerial sserial(10, 11);
 * SerialVAllocP<SoftwareSerial> valloc(SERIALRAM_POOLSIZE, 115200, &sserial);
 * @endcode
 *
 * __Communication with the RAM host__
 *
 * A special [Python](https://www.python.org/) script needs to be run on the RAM host
 * (PC, ARM board etc.) that communicates with the Arduino code. This script allocates a
 * buffer that is sufficient in size to act as RAM for the code running `virtmem`. Both this
 * script and SerialVAlloc::start will block and wait until a connection is established between the two.
 *
 * The serial script is stored in `virtmem/extras/serial_host.py`, and requires Python 3 as well
 * as the [pyserial module](https://pythonhosted.org/pyserial/).
 *
 * Running the script with the `-h` (or `--help`) option will display the available
 * commandline parameters for configuration:
 * @verbatim
usage: serial_host.py [-h] [-p PORT] [-b BAUD] [-l PASSDEV] [-r PASSBAUD]

 optional arguments:
 -h, --help            show this help message and exit
 -p PORT, --port PORT  serial device connected to the arduino. Default:
                       /dev/ttyACM0
 -b BAUD, --baud BAUD  Serial baudrate. Default: 115200
 -l PASSDEV, --pass PASSDEV
                       serial pass through device
 -r PASSBAUD, --passbaud PASSBAUD
                       baud rate of serial port pass through device. Default:
                       115200
 @endverbatim
 * The port (`-p` or `--port` option) should be set to the serial port connected
 * to the MCU running `virtmem` (e.g. /dev/ttyACM0, COM1 etc). The baudrate
 * (`-b` or `--baud` option) should match the configured baudrate of the allocator
 * (see SerialVAlloc::SerialVAllocP and SerialVAlloc::setBaudRate).
 *
 * The `serial_host.py` script allows to act as a serial bridge and can 'pass through' all
 * communication to and from the MCU to another (virtual) serial port. By setting up a
 * 'virtual serial port' (e.g. via [socat](http://www.dest-unreach.org/socat) or
 * [com0com](http://com0com.sourceforge.net)) and connecting `serial_host.py` to it,
 * other software is still able to communicate with your MCU. The (virtual) serial port
 * and its baudrate is set by the `-l` (or `--pass`) and `-r` (or `--passbaud`) options,
 * respectively.
 *
 * Some examples:
 * @code{.py}
 * serial_host.py # uses default serial port and baudrate
 * python serial_host.py -p COM2 # uses COM2 (Windows) as serial port, with default baudrate
 * serial_host.py -p /dev/ttyACM2 -b 9600 # uses /dev/ttyACM2 (Linux) as serial port, with 9600 baudrate
 * serial_host.py -l /dev/pts/1 # use default serial settings and pass all traffic through /dev/pts/1
 * @endcode
 *
 * Once the script has started it will keep monitoring the serial port until exited manually
 * (e.g. by pressing ctrl+C). Sending text can be done by simply writing the text and pressing
 * enter.
 *
 * __Sharing serial ports with other code__
 *
 * Sometimes it may be desired to use the serial port used by `SerialVAllocP` for other purposes.
 * Even when a port is 'shared', simply writing to it is no problem and dealt with by `serial_host.py`.
 * For instance, when `Serial.print()` is called (and Serial is bound to SerialVAllocP), the text
 * is simply printed to the terminal running `serial_host.py`.
 *
 * Dealing with user input, however, is more tricky. Any text that is typed in the terminal running
 * `serial_host.py` is still sent to the MCU running `virtmem`. To retrieve it, the
 * SerialVAllocP::input class must be used:
 * @code{.cpp}
 * SerialVAlloc valloc;
 * // ...
 * if (valloc.input.available())
 * {
 *     char c = valloc.input.read();
 *     // ...
 * }
 * @endcode
 * Note that in the above example it is more efficient to use the [availableAtLeast function](@ref virtmem::serram_utils::SerialInput::availableAtLeast).
 * For more information see the serram_utils::SerialInput class.
 *
 * @tparam IOStream The type of serial class to use for communication. Default is the type of
 * the Serial class.
 * @tparam Properties Allocator properties, see DefaultAllocProperties
 *
 * @sa @ref bUsing, SerialVAlloc
 */
template <typename IOStream=typeof(Serial), typename Properties=DefaultAllocProperties>
class SerialVAllocP : public VAlloc<Properties, SerialVAllocP<IOStream, Properties> >
{
    uint32_t baudRate;
    IOStream *stream;

    void doStart(void)
    {
        serram_utils::init(stream, baudRate, this->getPoolSize());
    }

    void doStop(void) { }

    void doRead(void *data, VPtrSize offset, VPtrSize size)
    {
//        uint32_t t = micros();
        serram_utils::sendReadCommand(stream, serram_utils::CMD_READ);
        serram_utils::writeUInt32(stream, offset);
        serram_utils::writeUInt32(stream, size);
        Serial.flush();
        serram_utils::readBlock(stream, (char *)data, size);
//        Serial.print("read: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

    void doWrite(const void *data, VPtrSize offset, VPtrSize size)
    {
//        const uint32_t t = micros();
        serram_utils::sendWriteCommand(stream, serram_utils::CMD_WRITE);
        serram_utils::writeUInt32(stream, offset);
        serram_utils::writeUInt32(stream, size);
        serram_utils::writeBlock(stream, (const uint8_t *)data, size);
//        Serial.print("write: "); Serial.print(size); Serial.print("/"); Serial.println(micros() - t);
    }

public:
    /**
     * @brief Handles input of shared serial connections.
     * @sa serram_utils::SerialInput, SerialVAllocP
     */
    serram_utils::SerialInput<IOStream> input;

    /**
     * @brief Constructs (but not initializes) the serial allocator.
     * @param ps The size of the virtual memory pool.
     * @param baud The baudrate.
     * @param s The instance variable of the serial class that should be used (e.g. Serial, Serial1)
     * @sa setBaudRate and setPoolSize
     */
    SerialVAllocP(VPtrSize ps=VIRTMEM_DEFAULT_POOLSIZE, uint32_t baud=115200, IOStream *s=&Serial) :
        baudRate(baud), stream(s), input(stream) { this->setPoolSize(ps); }

    // only works before start() is called
    /**
     * @brief Sets the baud rate.
     * @param baud The baudrate for the serial connection.
     * @note Only call this function when the allocator is not yet initialized (i.e. before calling @ref start)
     */
    void setBaudRate(uint32_t baud) { baudRate = baud; }

    /**
     * @brief Send a 'ping' to retrieve a response time. Useful for debugging.
     * @return Response time of serial script connected over serial, in microseconds.
     */
    uint32_t ping(void) const
    {
        serram_utils::sendReadCommand(stream, serram_utils::CMD_PING);
        const uint32_t starttime = micros();
        while (!serram_utils::waitForCommand(stream, serram_utils::CMD_PING, 1000))
            ;
        return micros() - starttime;
    }
};

typedef SerialVAllocP<> SerialVAlloc; //!< Shortcut to SerialVAllocP with default template arguments

/**
 * @example serial_simple.ino
 * This is a simple example sketch showing how to use the serial allocator.
 */
}

#endif // VIRTMEM_SERIAL_ALLOC_H

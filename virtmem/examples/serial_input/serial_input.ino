/*
 * This example shows how to handle serial input when using the serial virtual memory allocator
 * (SerialValloc/SerialVallocP) with a 'shared' serial port. In this example the default serial port,
 * ('Serial') is used.
 *
 * The only requirement is that a so called 'RAM host' (for instance a PC) is
 * connected via the default serial port (Serial). The RAM host should run the
 * 'script/serial_host.py' Python script.
 */


#include <Arduino.h>
#include <virtmem.h>
#include <alloc/serial_alloc.h>

const uint32_t poolSize = 1024l * 32l; // the size of the virtual memory pool (in bytes)

// pull in complete virtmem namespace
using namespace virtmem;

SerialVAlloc valloc(poolSize); // default settings: use Serial with 115200 baudrate

void setup()
{
    while (!Serial)
        ; // wait for serial to come up

    valloc.start();
}

void loop()
{
    if (valloc.input.availableAtLeast() > 0) // some input available?
    {
        // get the amount of bytes available.
        // NOTE: this function is the more 'expensive', but accurate version of
        // availableAtLeast().
        int bytes = valloc.input.available();

        Serial.print("read single byte: "); Serial.println(valloc.input.read());
        --bytes;

        if (bytes > 0) // more input?
        {
            // read it in a buffer: reading it in one go is much faster
            char buffer[128];

            bytes = max(bytes, 127); // make sure we don't read too much

            const int readbytes = valloc.input.readBytes(buffer, bytes);
            buffer[readbytes] = 0; // make sure the string is null terminated
            Serial.print("read string: "); Serial.println(buffer);
        }
    }
}

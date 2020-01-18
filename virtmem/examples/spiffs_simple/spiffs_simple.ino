/*
 * Minimal example showing how to use the SD virtual memory allocator
 * (SPIFFSVAlloc/SPIFFSVAllocP). This allocator uses a file on a FAT formatted SD card as RAM.
 *
 * Requirements:
 *  - the SdFat library should be installed (https://github.com/greiman/SdFat)
 *  - an FAT formatted SD card
 *  - a connection to the SD card via SPI
 */

#include <Arduino.h>
#include <virtmem.h>
#include <spiffs_alloc.h>

const uint32_t poolSize = 1024l * 32l; // the size of the virtual memory pool (in bytes)

// pull in complete virtmem namespace
using namespace virtmem;

SPIFFSVAlloc valloc(poolSize);

void setup()
{
    while (!Serial)
        ; // wait for serial to come up

    Serial.begin(115200);

    // initialize SdFat library: this should be done before starting the allocator!
    SPIFFS.begin();
    //SPIFFS.format();

    valloc.start();

    delay(3000); // add some delay so the user can connect with a serial terminal
}

void loop()
{
    // allocate some integer on virtual memory
    VPtr<int, SPIFFSVAlloc> vpi = valloc.alloc<int>();

    *vpi = 42; // assign some value, just like a regular pointer!
    Serial.print("*vpi = "); Serial.println(*vpi);

    valloc.free(vpi); // And free the virtual memory

    delay(1000); // keep doing this with 1 second pauses inbetween...
}

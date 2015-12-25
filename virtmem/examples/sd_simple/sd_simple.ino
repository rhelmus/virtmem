/*
 * Minimal example showing how to use the SD virtual memory allocator
 * (SDVAlloc/SDVAllocP). This allocator uses a file on a FAT formatted SD card as RAM.
 *
 * Requirements:
 *  - the SdFat library should be installed (https://github.com/greiman/SdFat)
 *  - an FAT formatted SD card
 *  - a connection to the SD card via SPI
 */


#include <Arduino.h>
#include <virtmem.h>
#include <SdFat.h>
#include <alloc/sd_alloc.h>

// configuration for SD
const int chipSelect = 9;
const uint32_t poolSize = 1024l * 32l; // the size of the virtual memory pool (in bytes)
const int spiSpeed = SPI_FULL_SPEED;

// pull in complete virtmem namespace
using namespace virtmem;

SdFat sd;
SDVAlloc valloc(poolSize);

void setup()
{
    // uncomment if using the ethernet shield
    // pinMode(10, OUTPUT); digitalWrite(10, HIGH);

    while (!Serial)
        ; // wait for serial to come up

    Serial.begin(115200);

    // initialize SdFat library: this should be done before starting the allocator!
    if (!sd.begin(chipSelect, spiSpeed))
        sd.initErrorHalt();

    valloc.start();

    delay(3000); // add some delay so the user can connect with a serial terminal
}

void loop()
{
    // allocate some integer on virtual memory
    VPtr<int, SDVAlloc> vpi = valloc.alloc<int>();

    *vpi = 42; // assign some value, just like a regular pointer!
    Serial.print("*vpi = "); Serial.println(*vpi);

    valloc.free(vpi); // And free the virtual memory

    delay(1000); // keep doing this with 1 second pauses inbetween...
}

/*
 * Example showing how the internal memory pages of an allocator can be configured.
 * These allocator properties describe the size and amount of memory pages are used. Since they
 * are stored in regular RAM, configuring allocator properties can be useful to limit the RAM
 * used by virtmem. On the other hand, if you have RAM to spare, increasing the size and/or amount
 * of memory pages can speed up virtmem. Increasing the number of pages typically improves
 * random access speeds, while increasing the size of memory pages typically reduces the amount
 * of swapping needed. For more information refer to the documentation on DefaultAllocProperties.
 *
 * This example uses the SD allocator, however, any other allocator could be configured in a similar
 * way.
 *
 * Requirements:
 *  - the SdFat library should be installed (https://github.com/greiman/SdFat)
 *  - an FAT formatted SD card
 *  - a connection to the SD card via SPI
 */

#include <Arduino.h>
#include <virtmem.h>
#include <SdFat.h>
#include <sd_alloc.h>

// configuration for SD
const int chipSelect = 9;
const uint32_t poolSize = 1024l * 32l; // the size of the virtual memory pool (in bytes)
const int spiSpeed = SPI_FULL_SPEED;

// pull in complete virtmem namespace
using namespace virtmem;

// struct containing the properties used to define the memory pages of an allocator.
// Note that all variables should be static and have a large enough (integer) type to contain the
// numeric value. By default every allocator uses the values defined from DefaultAllocProperties,
// defined in config.h.
struct AllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 64;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 128;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 512; // note: uint16_t to contain larger numeric value
};

typedef SDVAllocP<AllocProperties> Alloc; // shortcut

SdFat sd;
Alloc valloc(poolSize);

// rest is more or less the same as sd_simple example
// ...

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
    VPtr<int, Alloc> vpi = valloc.alloc<int>();

    *vpi = 42; // assign some value, just like a regular pointer!
    Serial.print("*vpi = "); Serial.println(*vpi);

    valloc.free(vpi); // And free the virtual memory

    delay(1000); // keep doing this with 1 second pauses inbetween...
}

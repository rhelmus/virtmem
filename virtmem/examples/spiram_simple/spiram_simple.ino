/*
 * Minimal example showing how to use the SPI RAM virtual memory allocator
 * (SPIRAMVAlloc/SPIRAMVAllocP). The 23LC series from Microchip are supported.
 *
 * Requirements:
 *  - the serialram library should be installed (https://github.com/rhelmus/serialram)
 *  - the SRAM chip should be properly connected with SPI (the CS pin is configured below)
 */


#include <Arduino.h>
#include <virtmem.h>
#include <serialram.h>
#include <alloc/spiram_alloc.h>

// configuration of SRAM chip: a 23LC1024 chip is assumed here which has CS connected to pin 9
const int chipSelect = 9;
const int chipSize = 1024 * 128; // 128 kB (=1 mbit)
const bool largeAddressing = true; // false if chipsize <1 mbit
const CSerialRam::ESPISpeed spiSpeed = CSerialRam::SPEED_FULL;

// pull in complete virtmem namespace
using namespace virtmem;

SPIRAMVAlloc valloc(chipSize, largeAddressing, chipSelect, spiSpeed);

void setup()
{
    while (!Serial)
        ; // wait for serial to come up

    Serial.begin(115200);
    valloc.start();

    delay(3000); // add some delay so the user can connect with a serial terminal
}

void loop()
{
    // allocate some integer on virtual memory
    VPtr<int, SPIRAMVAlloc> vpi = valloc.alloc<int>();

    *vpi = 42; // assign some value, just like a regular pointer!
    Serial.print("*vpi = "); Serial.println(*vpi);

    valloc.free(vpi); // And free the virtual memory

    delay(1000); // keep doing this with 1 second pauses inbetween...
}

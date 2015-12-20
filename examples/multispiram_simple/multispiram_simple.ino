/*
 * Minimal example showing how to use the multi SPI RAM virtual memory allocator
 * (MultiSPIRAMVAllocP). The 23LC series from Microchip are supported. In this example
 * two chips are used. The total memory of both chips will be combined by the allocator.
 *
 * Requirements:
 *  - the serialram library should be installed (https://github.com/rhelmus/serialram)
 *  - two SRAM chips should be properly connected with SPI (the CS pins are configured below)
 */


#include <Arduino.h>
#include <virtmem.h>
#include <serialram.h>
#include <spiram_alloc.h>

// pull in complete virtmem namespace
using namespace virtmem;

// configuration for two 23LC1024 chips (128 kByte in size), CS connected to pins 9 & 10
SPIRamConfig scfg[2] =
{
    // format: <large adressing> (true if > 512 kbit), <size of the chip>, <CS pin>, <SPI speed>
    { true, 1024 * 128, 9, CSerialRam::SPEED_FULL },
    { true, 1024 * 128, 10, CSerialRam::SPEED_FULL }
};

typedef MultiSPIRAMVAllocP<scfg, 2> Alloc; // shortcut
Alloc valloc;

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
    VPtr<int, Alloc> vpi = valloc.alloc<int>();

    *vpi = 42; // assign some value, just like a regular pointer!
    Serial.print("*vpi = "); Serial.println(*vpi);

    valloc.free(vpi); // And free the virtual memory

    delay(1000); // keep doing this with 1 second pauses inbetween...
}

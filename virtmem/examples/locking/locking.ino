/*
 * Example showing how to use virtual data locks. When a lock is made a portion of virtual memory
 * is kept within a virtual memory page until the lock is released. Locking data can significantly
 * improve performance since data can be accessed through a regular pointer, and helps dealing
 * with code that only accepts regular pointers. The latter is demonstarted in this example.
 *
 * This example uses the SD allocator, however, any other allocator could be used as well.
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
const int chipSelect = 4;
const uint32_t poolSize = 1024l * 328l; // the size of the virtual memory pool (in bytes)
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

    Serial.println("Initialized!");
}

void loop()
{
    // allocate a string in virtual memory
    VPtr<char, SDVAlloc> vstr = valloc.alloc<char>(128);

    strcpy(vstr, "hello (virtual) world!");

    // print the string: for this we want to use Serial.print(), however, this function
    // only accepts 'regular' strings. Hence, we have to lock the data first.

    int size = strlen(vstr) + 1; // size to lock (including string zero terminator)
    VPtr<char, SDVAlloc> p = vstr;
    while (size)
    {
        // Make the lock: the last argument specifies the lock should be read-only (more efficient)
        // Since we only have to print the string we set this to true.
        VPtrLock<VPtr<char, SDVAlloc> > lock = makeVirtPtrLock(p, size, true);

        // Get the actual size of the lock. This is important, because it can happen that
        // the complete data could not be locked at once.
        const int lockedsize = lock.getLockSize();

        // dereferencing the lock (*lock) will return a regular pointer to the data
        Serial.write(*lock, lockedsize);

        size -= lockedsize;
        p += lockedsize;

        // in case lockedsize was smaller than size, the above code will be called again using the
        // remaining string.
    }

    Serial.println(""); // end with a newline

    valloc.free(vstr); // And free the virtual memory

    delay(1000); // keep doing this with 1 second pauses inbetween...
}

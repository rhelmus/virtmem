#include <Arduino.h>
#include <virtmem.h>
#include "benchmark.h"

//#define RUN_STATICALLOC
//#define RUN_SPIRAMALLOC
//#define RUN_NATIVE
#define RUN_SERIALALLOC
//#define RUN_SDALLOC

// uncomment to disable a SPI select pin, useful when using ethernet shield
#define DISABLE_SELECTPIN 10

#define NATIVE_BUFSIZE 1024 * 4
#define NATIVE_REPEATS 100

#define STATICALLOC_POOLSIZE 1024 * 32 + 128 // plus some size overhead
#define STATICALLOC_BUFSIZE 1024 * 32
#define STATICALLOC_REPEATS 100

#define SPIRAM_POOLSIZE 1024 * 128
#define SPIRAM_BUFSIZE 1024 * 127
#define SPIRAM_REPEATS 5
#define SPIRAM_CSPIN 9

#define SERIALRAM_POOLSIZE 1024l * 1024l
#define SERIALRAM_BUFSIZE 1024l * 128l
#define SERIALRAM_REPEATS 5

#define SD_POOLSIZE 1024l * 1024l
#define SD_BUFSIZE 1024l * 128l
#define SD_REPEATS 5
#define SD_CSPIN 9
#define SD_SPISPEED SPI_FULL_SPEED


#ifdef RUN_STATICALLOC
#include <alloc/static_alloc.h>
StaticVAllocP<STATICALLOC_POOLSIZE> staticAlloc;
#endif

#ifdef RUN_SPIRAMALLOC
#include <SPI.h>
#include <alloc/spiram_alloc.h>
#include <serialram.h>
SPIRAMVAlloc SPIRamAlloc(SPIRAM_POOLSIZE, true, SPIRAM_CSPIN, CSerialRam::SPEED_FULL);
#endif

#ifdef RUN_SERIALALLOC
#include <alloc/serial_alloc.h>
SerialVAlloc serialRamAlloc(SERIALRAM_POOLSIZE, 115200);
#endif

#ifdef RUN_SDALLOC
#include <SdFat.h>
#include <alloc/sd_alloc.h>
SDVAlloc SDRamAlloc(SD_POOLSIZE);
SdFat sd;
#endif


#ifdef RUN_NATIVE
void runNativeBenchmark(uint32_t bufsize, uint8_t repeats)
{
    volatile char buf[bufsize];

    printBenchStart(bufsize, repeats);

    const uint32_t time = millis();
    for (uint8_t i=0; i<repeats; ++i)
    {
        for (uint32_t j=0; j<bufsize; ++j)
            buf[j] = (char)j;
    }

    printBenchEnd(millis() - time, bufsize, repeats);
}
#endif


void setup()
{
#ifdef DISABLE_SELECTPIN
    pinMode(DISABLE_SELECTPIN, OUTPUT);
    digitalWrite(DISABLE_SELECTPIN, HIGH);
#endif

#ifndef RUN_SERIALALLOC
    while (!Serial)
        ;

    Serial.begin(115200);

    delay(3000);
#endif

#ifdef RUN_SDALLOC
    if (!sd.begin(SD_CSPIN, SD_SPISPEED))
        sd.initErrorHalt();
#endif
}

void loop()
{
#ifdef RUN_NATIVE
    Serial.println("Running native...\n");
    runNativeBenchmark(NATIVE_BUFSIZE, NATIVE_REPEATS);
    Serial.println("\nDone!");
#endif

#ifdef RUN_STATICALLOC
    Serial.println("Running static allocator...\n");
    runBenchmarks(staticAlloc, STATICALLOC_BUFSIZE, STATICALLOC_REPEATS);
    Serial.println("\nDone!");
#endif

#ifdef RUN_SPIRAMALLOC
    Serial.println("Running SPI ram allocator...\n");
    runBenchmarks(SPIRamAlloc, SPIRAM_BUFSIZE, SPIRAM_REPEATS);
    Serial.println("\nDone!");
#endif

#ifdef RUN_SERIALALLOC
    Serial.println("Running serial ram allocator...\n");
    runBenchmarks(serialRamAlloc, SERIALRAM_BUFSIZE, SERIALRAM_REPEATS);
    Serial.println("\nDone!");
#endif

#ifdef RUN_SDALLOC
    Serial.println("Running sd fat allocator...\n");
    runBenchmarks(SDRamAlloc, SD_BUFSIZE, SD_REPEATS);
    Serial.println("\nDone!");
#endif

    delay(2000);
}

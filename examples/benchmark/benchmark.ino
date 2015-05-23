#include <Arduino.h>
#include <virtmem.h>
#include "benchmark.h"

using namespace virtmem;

//#define RUN_STATICALLOC
//#define RUN_SPIRAMALLOC
//#define RUN_NATIVE
#define RUN_SERIALALLOC


#define NATIVE_BUFSIZE 1024 * 4
#define NATIVE_REPEATS 100

#define STATICALLOC_POOLSIZE 1024 * 6 + 128 // plus some size overhead
#define STATICALLOC_BUFSIZE 1024 * 6
#define STATICALLOC_REPEATS 100

#define SPIRAM_POOLSIZE 1024 * 128
#define SPIRAM_BUFSIZE 1024 * 127
#define SPIRAM_REPEATS 5
#define SPIRAM_CSPIN 9

#define SERIALRAM_POOLSIZE 1024l * 128l
#define SERIALRAM_BUFSIZE 1024l * 6l
#define SERIALRAM_REPEATS 5


#ifdef RUN_STATICALLOC
#include <static_alloc.h>
CStaticVirtMemAlloc<STATICALLOC_POOLSIZE> staticAlloc;
#endif

#ifdef RUN_SPIRAMALLOC
#include <SPI.h>
#include <spiram_alloc.h>
#include <serialram.h>
CSPIRAMVirtMemAlloc<> SPIRamAlloc(SPIRAM_POOLSIZE, true, SPIRAM_CSPIN, CSerialRam::SPEED_FULL);
#endif

#ifdef RUN_SERIALALLOC
#include <serram_alloc.h>
CSerRAMVirtMemAlloc<> serialRamAlloc(SERIALRAM_POOLSIZE, 115200);
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
#if 0
    while (!Serial)
        ;

    Serial.begin(115200);

    delay(3000);
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

    delay(2000);
}

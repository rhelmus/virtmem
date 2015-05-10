#ifndef VIRTMEM_CONFIG_H
#define VIRTMEM_CONFIG_H

#include <stdint.h>

#define VIRTMEM_WRAP_CPOINTERS
#define VIRTMEM_TRACE_STATS

enum
{
    // Used for allocators with variable sized pools (sdfatlib, stdio, serram etc)
    DEFAULT_POOLSIZE = 1024 * 1024,
};


// Default virtual memory page settings
// NOTE: Take care of sufficiently large int types when increasing these values

#if defined(__MK20DX256__) || defined(__SAM3X8E__) // Teensy 3.1 / Arduino Due (>= 64 kB sram)
struct SDefaultAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 64;
    static const uint8_t mediumPageCount = 4;
    static const uin16_t mediumPageSize = 256;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 1024 * 4;
};
#elif defined(__MK20DX128__) // Teensy 3.0 (16 kB sram)
struct SDefaultAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 32;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 128;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 1024 * 1;
};
// Teensy LC / Arduino mega (8 kB sram)
#elif defined(__MKL26Z64__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
struct SDefaultAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 32;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 128;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 512 * 1;
};
// PC like platform
#elif defined(__unix__) || defined(__UNIX__) || (defined(__APPLE__) && defined(__MACH__)) || defined(_WIN32)
struct SDefaultAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 64;
    static const uint8_t mediumPageCount = 4;
    static const uint16_t mediumPageSize = 256;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 1024 * 32;
};
#else
// Small AVR like MCUs (e.g. Arduino Uno) or unknown platform. In the latter case these settings
// are considered as a safe default.

// Not a small AVR?
#if !defined(__AVR_ATmega168__) && !defined(__AVR_ATmega328P__) && !defined(__AVR_ATmega32U4__)
#warning "Unknown platform. You probably want to change virtual memory page settings."
#endif

struct SDefaultAllocProperties
{
    static const uint8_t smallPageCount = 2, smallPageSize = 16;
    static const uint8_t mediumPageCount = 1, mediumPageSize = 32;
    static const uint8_t bigPageCount = 1, bigPageSize = 128;
};


// UNDONE
#endif


#endif // CONFIG_H

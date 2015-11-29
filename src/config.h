#ifndef VIRTMEM_CONFIG_H
#define VIRTMEM_CONFIG_H

/**
  @file
  @brief This header file contains several variables that can be used to customize virtmem.
  */

#include <stdint.h>

/**
  @brief If defined, enable wrapping of regular pointers inside virtmem::VPtr.
  @sa virtmem::VPtr::wrap
  */
#define VIRTMEM_WRAP_CPOINTERS

/**
  * @brief If defined, the "address of" operator (`&`) of VPtr will be overloaded to
  * return a virtual pointer that has its own address wrapped.
  *
  * This is useful to allow double pointers, for example:
  * @code
  * typedef virtmem::VPtr<int, SDVAlloc> vptrType;
  * vptrType vptr;
  * virtmem::VPtr<vptrType, SDVAlloc> vptrptr = &vptr;
  * @endcode
  */
#define VIRTMEM_VIRT_ADDRESS_OPERATOR

/**
  @brief If defined, several functions in the allocator will be defined that can be used to
  access statistics such as memory usage and page swaps.
  @see \ref statf "Statistics functions"
  */
#define VIRTMEM_TRACE_STATS

/**
  @brief The default poolsize for allocators supporting a variable sized pool.

  This value is used for variable sized allocators, such as SDVAlloc and
  SerialVAlloc.
  */
#define DEFAULT_POOLSIZE 1024l * 1024l

namespace virtmem {

// Default virtual memory page settings
// NOTE: Take care of sufficiently large int types when increasing these values

#if defined(__MK20DX256__) || defined(__SAM3X8E__) // Teensy 3.1 / Arduino Due (>= 64 kB sram)
struct DefaultAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 64;
    static const uint8_t mediumPageCount = 4;
    static const uint16_t mediumPageSize = 256;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 1024 * 1;
};
#elif defined(__MK20DX128__) // Teensy 3.0 (16 kB sram)
struct DefaultAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 32;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 128;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 512 * 1;
};
// Teensy LC / Arduino mega (8 kB sram)
#elif defined(__MKL26Z64__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
struct DefaultAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 32;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 128;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 512 * 1;
};
// PC like platform
#elif defined(__unix__) || defined(__UNIX__) || (defined(__APPLE__) && defined(__MACH__)) || defined(_WIN32)
struct DefaultAllocProperties
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

struct DefaultAllocProperties
{
    static const uint8_t smallPageCount = 2, smallPageSize = 16;
    static const uint8_t mediumPageCount = 1, mediumPageSize = 32;
    static const uint8_t bigPageCount = 1, bigPageSize = 128;
};

#endif

/**
  @struct DefaultAllocProperties
  @brief This struct contains default parameters for virtual memory pages.

  The fields in this struct define the amount- and size of the *small*, *medium* and *big*
  memory pages for an allocator. The defaults are platform dependend and can be changed in
  config.h. Alternatively, page settings can be set by defining a customized structure
  and passing this structure as a template parameter to an allocator.

  Example:
  @code
// This struct contains a customized set of memory page properties.
// While the datatype of each member does not matter, all members must be static.
struct MyAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 64;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 128;
    static const uint8_t bigPageCount = 4,
    static const uint16_t bigPageSize = 512; // note: uint16_t to contain larger numeric value
};

// Create allocator with customized page properties
SDVAlloc<MyAllocProperties> alloc;
  @endcode

  @var DefaultAllocProperties::smallPageCount
  @brief The number of *small* pages. @hideinitializer
  @var DefaultAllocProperties::mediumPageCount
  @brief The number of *medium* pages. @hideinitializer
  @var DefaultAllocProperties::bigPageCount
  @brief The number of *big* pages. @hideinitializer

  @var DefaultAllocProperties::smallPageSize
  @brief The size of a *small* page. @hideinitializer
  @var DefaultAllocProperties::mediumPageSize
  @brief The size of a *medium* page. @hideinitializer
  @var DefaultAllocProperties::bigPageSize
  @brief The size of a *big* page. @hideinitializer
  */

}

#endif // CONFIG_H

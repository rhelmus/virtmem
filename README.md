Main
=========

# Introduction
`virtmem` is an Arduino library that allows your project to easily use an
external memory source to extend the (limited) amount of available RAM. This
library supports several memory resources, for instance, SPI ram (e.g. the
`23LC1024` chip from Microchip), an SD card or even a computer connected via a
serial connection. The library is made in such a way that managing and using
this _virtual memory_ closely resembles working with data from 'normal' memory.

# Features
* Extend the available memory with kilobytes, megabytes or even gigabytes
* Supports SPI RAM (23LC series from Microchip), SD cards and RAM from a computer connected through serial
* Easy C++ interface that closely resembles regular data access
* Memory page system to speed up access to virtual memory
* New memory interfaces can be added easily
* Code is mostly platform independent and can fairly easy be ported to other
plaforms (x86 port exists for debugging)

# Demonstration
~~~{.cpp}
#include <Arduino.h>
#include <SdFat.h>
#include <virtmem.h>
#include <alloc/sd_alloc.h>

// Simplify virtmem usage
using namespace virtmem;

// Create virtual a memory allocator that uses SD card (with FAT filesystem) as virtual memory pool
// The default memory pool size (1 MB) is used.
SDVAlloc valloc;

SdFat sd;

struct MyStruct { int x, y; };

void setup()
{
    // Initialize SdFatlib
    if (!sd.begin(9, SPI_FULL_SPEED))
        sd.initErrorHalt();

    valloc.start(); // Always call this to initialize the allocator before using it

    // Allocate a char buffer of 10000 bytes in virtual memory and store the address to a virtual pointer
    VPtr<char, SDVAlloc> str = valloc.alloc<char>(10000);

    // Set the first 1000 bytes to 'A'
    memset(str, 'A', 1000);

    // array access
    str[1] = 'B';

    // Own types (structs/classes) also work.
    VPtr<MyStruct, SDVAlloc> ms = valloc.alloc<MyStruct>(); // alloc call without parameters: use automatic size deduction
    ms->x = 5;
    ms->y = 15;
}

void loop()
{
    // ...
}
~~~

This Arduino sketch demonstrates how to use a SD card as virtual memory
store. By using a virtual memory pointer wrapper class, using virtual memory
becomes quite close to using data residing in 'normal' memory.

# Manual
The manual [can be found here](http://rhelmus.github.io/virtmem/index.html).

# Benchmark
Some benchmarking results are shown below. Note that these numbers are generated with very simple,
and possibly not so accurate tests, hence they should only be used as a rough indication.

<table>
    <tr>
        <th>Read / Write speed (kB/s)
        <th align="center">Teensy 3.2 (96 MHz)
        <th align="center">Teensy 3.2 (144 MHz)
        <th align="center">Arduino Uno
    <tr>
        <td>Serial
        <td align="center">500 / 373
        <td align="center">496 / 378
        <td align="center">30 / 20
    <tr>
        <td>SD
        <td align="center">1107 / 98
        <td align="center">1102 / 91
        <td align="center">156 / 44
    <tr>
        <td>SPI RAM
        <td align="center">1887 / 1159
        <td align="center">2083 / 1207
        <td align="center">150 / 118
</table>

Some notes:
- Serial: Virtual means that a USB serial connection is used, which is only limited by USB speeds.
- SD/SPI RAM: measured at maximum SPI speeds. For SPI RAM a 23LCV1024 chip was used.
- More details in [the manual](http://rhelmus.github.io/virtmem/index.html#bench).

Manual
========

[TOC]


# Introduction {#intro}
`virtmem` is an Arduino library that allows your project to easily use an
external memory source to extend the (limited) amount of available RAM. This
library supports several memory resources, for instance, SPI ram (e.g. the
`23LC1024` chip from Microchip), an SD card or even a computer connected via a
serial connection. The library is made in such a way that managing and using
this _virtual memory_ closely resembles working with data from 'normal' memory.

# Features {#features}
* Extend the available memory with kilobytes, megabytes or even gigabytes
* Supports SPI RAM (23LC series from Microchip), SD cards and RAM from a computer connected through serial
* Easy C++ interface that closely resembles regular data access
* Memory page system to speed up access to virtual memory
* New memory interfaces can be added easily
* Code is mostly platform independent and can fairly easy be ported to other
plaforms (x86 port exists for debugging)

# Demonstration {#demo}
Before delving into specifics, here is a simple example to demonstrate how `virtmem` works and what it can do.

~~~{.cpp}
#include <Arduino.h>
#include <SdFat.h>
#include <virtmem.h>
#include <sdfatlib_alloc.h>

// Simplify virtmem usage
using namespace virtmem;

// Create virtual memory allocator that uses SD card (with FAT filesystem) as virtual memory pool
// The default memory pool size (1 MB) is used.
CSdfatlibVirtMemAlloc<> valloc;

SdFat sd;

struct MyStruct { int x, y; };

void setup()
{
    // Initialize SdFatlib
    if (!sd.begin(9, SPI_FULL_SPEED))
        sd.initErrorHalt();

    valloc.start(); // Always call this to initialize the allocator before using it

    // Allocate a char buffer of 10000 bytes in virtual memory and store the address to a virtual pointer
    TSdfatlibVirtPtr<char>::type str = str.alloc(10000);

    // Set the first 1000 bytes to 'A'
    memset(str, 'A', 1000);

    // array access
    str[1] = 'B';

    // Own types (structs/classes) also work.
    TSdfatlibVirtPtr<MyStruct>::type ms = ms.alloc(); // alloc call without parameters: use automatic size deduction
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

# Basics {#basics}

## Virtual memory {#bVirtmem}
As the name suggests, `virtmem` works similar to [virtual memory management found on computers](https://en.wikipedia.org/wiki/Virtual_memory).

The library uses a _paging system_ to work efficiently with virtual memory. A
_memory page_ is a static buffer that resides in regular RAM and contains a
copy of a part of the total virtual memory pool. Multiple pages are typically
used (usually four), where each page contains a copy of a different virtual
memory region (see figure below).

<image here>

Whenever data is requested from virtual memory, the library first checks
whether the requested data resides in one of the memory pages, and if not, the
library will copy the data from virtual memory to a free memory page. Any data
access to this data now happens through a (regular) pointer address in the
memory page.

<image here>

When virtual memory has to be copied to a memory page, but no pages are free,
the library will first have to free a page. During this process any modified
data will be written back to the virtual memory pool and the requested data
will be loaded to the page. This process is sometimes called _swapping_.
Swapping a page is generally a relative time consuming process, since a large
amount of data has to be transferred for example over SPI. To minimize
swapping, the library always tries to free pages that were not recently loaded in
(FIFO like). The time spent on swapping is further reduced by having multiple
memory pages and only writing out data that was modified.

Because memory pages reside in regular RAM, (repeated) data access to paged
memory is quite fast providing the data has been loaded.

## Using virtual memory {#bUsing}

A virtual memory allocator is responsible for allocating, releasing and paging
of virtual memory. Several allocators are provided by `virtmem`, such as
[heuh](@ref virtmem::CSdfatlibVirtMemAlloc) and @ref CSPIRAMVirtMemAlloc. All allocator classes are
_singleton_, meaning that only one (global) instance should be defined
(however, instances of _different_ allocators can co-exist).




A virtual memory allocator is responsible for allocating, releasing and the
paging of virtual memory. The classes CBaseVirtMemAlloc and CVirtMemAlloc serve as base classes for e.g. C
define functions to configure the allocator and manage raw memory access.
Besides (de)allocation of memory, CBaseVirtMemAlloc also defines functions for
reading and writing raw memory (@see CBaseVirtMemAlloc::read and
CBaseVirtMemAlloc::write).

To use the

# Advanced {#advanced}

## Wrapping regular pointers {#wrapping}

## Locking virtual data {#locking}
A portion of the virtual memory can be locked to a memory page. Whenever such a
lock is made, the data is _guaranteed_ to stay in a memory page and will not be
swapped out. The same data can be locked multiple times, and the data may only
be swapped when all locks are released.

One reason to lock data is to improve performance. As soon as data is locked it
can safely be accessed through a regular pointer, meaning that no additional
overhead exists to use the data.

Another reason to lock data is to work with code that only accepts data
residing in regular memory space. For instance, if a function needs to be
called that requires a pointer to the data, the pointer to the locked memory
page region can be passed as argument.


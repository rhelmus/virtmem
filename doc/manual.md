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

// Create virtual a memory allocator that uses SD card (with FAT filesystem) as virtual memory pool
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
memory region. Similar to data allocated from the heap, data access from
virtual memory happens through a _virtual pointer_.

![virtual memory scheme](intro-scheme.png)

Whenever a virtual pointer is dereferenced, the library first checks whether
the requested data resides in one of the memory pages, and if not, the library
will copy the data from virtual memory to a suitable memory page. All data
access now happens through this memory page.

When virtual memory has to be copied to a memory page and no pages are free,
the library will first have to free a page. During this process any modified
data will be written back to the virtual memory pool and the requested data
will be loaded to the page. This process is sometimes called _swapping_.
Swapping a page is generally a relative time consuming process, since a large
amount of data has to be transferred for example over SPI. To minimize
swapping, the library always tries to free pages that were not recently loaded
in (FIFO like). The time spent on swapping is further reduced by having
multiple memory pages and only writing out data that was modified.

Because memory pages reside in regular RAM, (repeated) data access to paged
memory is quite fast.

## Using virtual memory {#bUsing}

Virtual memory in `virtmem` is managed by a virtual memory allocator. These are
C++ template classes which are responsible for allocating and releasing virtual
memory and contain the datablocks utilized for memory pages. Most of this
functionality is defined in the virtmem::CBaseVirtMemAlloc and
virtmem::CVirtMemAlloc classes. In addition to this, several allocator classes
are derived from these base classes that actually implement the code necessary
to deal with virtual memory (e.g. reading and writing data). For example, the
class virtmem::CSdfatlibVirtMemAlloc implements an allocator that uses an SD
card as a virtual memory pool. Note that all allocator classes are _singleton_,
meaning that only one (global) instance should be defined (however, instances
of _different_ allocators can co-exist, see @ref aMultiAlloc).

After defining a (global) instance of the allocator of choice, one of the first
things to do is to initialize it:

~~~{.cpp}
// Create a virtual memory allocator that uses SD card (with FAT filesystem) as virtual memory pool
// The default memory pool size (1 MB) is used.
virtmem::CSdfatlibVirtMemAlloc<> valloc;

// ...

void setup()
{
    // Initialize SdFatlib

    valloc.start(); // Always call this to initialize the allocator before using it

    // ...
}
~~~

Please note that, since this example uses the SD fat lib allocator, SD fat lib has
to be initialized prior to the allocator (see virtmem::CSdfatlibVirtMemAlloc).

Two interfaces exist to actually use virtual memory.

The first approach is to use interface with raw memory directly through
functions defined in virtmem::CBaseVirtMemAlloc (e.g. [read()](@ref virtmem::CBaseVirtMemAlloc::read) and
[write()](@ref virtmem::CBaseVirtMemAlloc::write)). Although dealing with raw memory might be
slightly more efficient performance wise, this approach is not recommended as
it is fairly cumbersome to do so.

The second approach is to use _virtual pointer wrappers_. These template
classes were designed to make virtual memory access as close as 'regular'
memory access as possible. Here is an example:

~~~{.cpp}
// define virtual pointer linked to SD fat memory
virtmem::CVirtPtr<int, virtmem::CSdfatlibVirtMemAlloc> vptr;
// virtmem::TSdfatlibVirtPtr<int>::type vptr; // same, but slightly shorter syntax

vptr = vptr.alloc(); // allocate memory to store integer (size automatically deduced from type)
*vptr = 4;
~~~

In this example we defined a virtual pointer to an `int`. Defining virtual
pointer variables can be done straight from virtmem::CVirtPtr or from one of
the shortcut helper classes (such as virtmem::TSdfatlibVirtPtr). Either do the
same.

Memory allocation ([alloc()](@ref virtmem::CVirtPtr::alloc)) is done through a (static)
function defined in the virtual pointer class. In the above example no
arguments were passed to [alloc()](@ref virtmem::CVirtPtr::alloc), which means that
`alloc` will automatically deduce the size required for the pointer type
(`int`). If you want to allocate a different size (for instance to use the data
as an array) then the number of bytes should be specified as the first argument
to `alloc`:

~~~{.cpp}
vptr = vptr.alloc(1000 * sizeof(int)); // allocate memory to store array of 1000 integers
vptr[500] = 1337;
~~~

Besides all standard types (char, int, short, etc.), virtual pointers can also
work with custom types (structs/classes):

~~~{.cpp}
struct MyStruct { int x, y; };
// ...
TSdfatlibVirtPtr<MyStruct>::type ms = ms.alloc();
ms->x = 5;
ms->y = 15;
~~~
Note that there are a few imitations when using structs (or classes) with `virtmem`. For details: see virtmem::CVirtPtr.

Finally, to free memory the [free()](@ref virtmem::CVirtPtr::free) function can be used:

~~~{.cpp}
vptr.free(vptr); // memory size automatically deduced
~~~

For further info see virtmem::CBaseVirtPtr and virtmem::CVirtPtr.

# Advanced {#advanced}

## Wrapping regular pointers {#aWrapping}

## Locking virtual data {#aLocking}
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
page region can then be passed to such a function.

### Note on memory pages {#alPages}
Each virtual data lock will essentially block a memory page. Since the number
of memory pages is rather small, care should be taken to not to create too many
different data locks.

To decrease the likelyhood of running out of free memory pages, `virtmem`
supports two additional sets of smaller memory pages which are specifically
used for data locking. They are much smaller in size, so they will not use a
large deal of RAM, while still providing extra capacity for smaller data locks.
Having a set of smaller memory pages is especially useful for
[automatic locking of data](@ref alAuto).

The default size and amount of memory pages is dependent upon the MCU platform
and can be customized as described in @ref aConfigAlloc.

### Using virtual data locks {#alUsing}
To create a lock to virtual memory the virtmem::CVirtPtrLock class is used:

~~~{.cpp}
typedef virtmem::TSdfatlibVirtPtr<char>::type virtCharPtr; // shortcut
virtCharPtr vptr = vptr.alloc(100); // allocate some virtual memory
virtmem::CVirtPtrLock<virtCharPtr> lock = virtmem::makeVirtPtrLock(vptr, 100, false);
memset(*lock, 10, 100); // set all bytes to '10'
~~~

The virtmem::makeVirtPtrLock function is used to create a lock. The last
parameter to this function (optional, by default `false`) tells whether the
locked data should be treated as read-only: if set to `false` the data will be
written back to the virtual memory (even if unchanged). If you know the data
will not be changed (or you don't care about changes), then it's more efficient
to pass `true` instead.

Accessing locked data is simply done by dereferencing the lock variable (i.e. `*lock`).

Sometimes it is not possible to completely lock the memory region that was
requested. For instance, there may not be sufficient space available to lock
the complete data to a memory page or there will be overlap with another locked
memory region. For this reason, it is **important to always check the _actual
size_ that was locked**. After a lock has been made, the effective size of the
locked region can be requested by the virtmem::CVirtPtrLock::getLockSize()
function. Because it is rather unpredictable whether the requested data can be
locked in one go, it is best create a loop that iteratively creates locks until
all bytes have been dealt with:

~~~{.cpp}
typedef virtmem::TSdfatlibVirtPtr<char>::type virtCharPtr; // shortcut
const int size = 10000;
int sizeleft = size;

virtCharPtr vptr = vptr.alloc(size); // allocate a large block of virtual memory
virtCharPtr p = vptr;

while (sizeleft)
{
    // create a lock to (a part) of the buffer
    virtmem::CVirtPtrLock<virtCharPtr> lock = virtmem::makeVirtPtrLock(p, sizeleft, false);

    const int lockedsize = lock.getLockSize(); // get the actual size of the memory block that was locked
    memset(*l, 10, lockedsize);

    p += lockedsize; // increase pointer to next block to lock
    sizeleft -= lockedsize; // decrease number of bytes to still lock
}

~~~
Note that a `memset` overload is provided by `virtmem` which works with virtual pointers.

After you are finished working with a virtual memory lock it has to be
released. This can be done manually with the virtmem::CVirtPtrLock::unlock
function. However, the virtmem::CVirtPtrLock destructor will call this function
automatically (the class follows the
[RAII principle](https://en.wikipedia.org/wiki/Resource_Acquisition_Is_Initialization)).
This explains why calling `unlock()` in the above example was not necessary, as
the destructor will call it automatically when the `lock` variable goes out of
scope at the end of every iteration.

### Automatic data locks {#alAuto}


## Multiple allocators {#aMultiAlloc}

## Configuring allocators {#aConfigAlloc}

# Examples {#examples}

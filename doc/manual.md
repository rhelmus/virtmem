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

More examples can be found on the <a href="examples.html">examples page</a>.

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

## File structure {#bStructure}
The `virtmem/` subdirectory contains all the code needed to compile Arduino sketches, and
should therefore be copied [as any other library](https://www.arduino.cc/en/Guide/Libraries).
The file layout follows the [new Arduino library specification](https://github.com/arduino/Arduino/wiki/Arduino-IDE-1.5:-Library-specification).

Other files include documentation and code for testing the library, and are not needed for compilation.
A summarized overview is given below.

Directory | Contents
--------- | --------
\<root\> | Internal development and README
benchmark/ | Internal code for benchmarking
doc/ | Files used for Doxygen created documentation
doc/html/ | This manual
gtest/ and test/ | Code for internal testing
virtmem/ | Library code
virtmem/extras/ | Contains python scripts needed for [the serial memory allocator](@ref virtmem::SerialVAlloc).

## Using virtual memory (tutorial) {#bUsing}

Virtual memory in `virtmem` is managed by a virtual memory allocator. These are
C++ template classes which are responsible for allocating and releasing virtual
memory and contain the datablocks utilized for memory pages. Most of this
functionality is defined in the virtmem::BaseVAlloc and
virtmem::VAlloc classes. In addition to this, several allocator classes
are derived from these base classes that actually implement the code necessary
to deal with virtual memory (e.g. reading and writing data). For example, the
class virtmem::SDVAlloc implements an allocator that uses an SD
card as a virtual memory pool.

The `virtmem` library supports the following allocators:
Allocator | Description | Header
----------|-------------|-------
virtmem::SDVAlloc | uses a FAT formatted SD card as memory pool | \c \#include <alloc/sd_alloc.h>
virtmem::SPIRAMVAlloc | uses SPI ram (Microchip's 23LC series) as memory pool | \c \#include <alloc/spiram_alloc.h>
virtmem::MultiSPIRAMVAllocP | like virtmem::SPIRAMVAlloc, but supports multiple memory chips | \c \#include <alloc/spiram_alloc.h>
virtmem::SerialVAlloc | uses RAM from a computer connected through serial as memory pool | \c \#include <alloc/serial_alloc.h>
virtmem::StaticVAllocP | uses regular RAM as memory pool (for debugging) | \c \#include <alloc/static_alloc.h>
virtmem::StdioVAlloc | uses files through regular stdio functions as memory pool (for debugging on PCs) | \c \#include <alloc/stdio_alloc.h>

The following code demonstrates how to setup a virtual memory allocator:

~~~{.cpp}
#include <Arduino.h>
#include <virtmem.h>
#include <SdFat.h>
#include <alloc/sd_alloc.h>

// Create a virtual memory allocator that uses SD card (with FAT filesystem) as virtual memory pool
// The default memory pool size (defined by VIRTMEM_DEFAULT_POOLSIZE in config.h) is used.
virtmem::SDVAlloc valloc;

// ...

void setup()
{
    // Initialize SdFatlib

    valloc.start(); // Always call this to initialize the allocator before using it

    // ...
}
~~~

To use the `virtmem` library you should include the `virtmem.h` header file. Furthermore, the specific
header file of the allocator has to be included (alloc/sd_alloc.h, see table above). Finally, since some allocators
depend on other libraries, they also may need to be included (SdFat.h in this example).

All classes, functions etc. of the `virtmem` library resides in the [virtmem namespace](@ref virtmem).
If you are unfamiliar with namespaces, you can find some info [here](http://www.cplusplus.com/doc/tutorial/namespaces/).
This is purely for 'organizational purposes', however, for small programs it may be easier to simply
pull the `virtmem` namespace in the global namespace:

~~~{.cpp}
// as above ...

using namespace virtmem; // pull in global namespace to shorten code
SDVAlloc valloc;

// as below ...
~~~

All allocator classes are _singleton_, meaning that only one (global) instance can be defined
(however, instances of _different_ allocators can co-exist, see @ref aMultiAlloc). Before the allocator
can be used it should be initialized by calling its [start function](@ref virtmem::BaseVAlloc::start).
Please note that, since this example uses the SD fat lib allocator, SD fat lib has
to be initialized prior to the allocator (see virtmem::SDVAlloc).

Two interfaces exist to actually use virtual memory.

The first approach is to interface with raw memory directly through
functions defined in virtmem::BaseVAlloc (e.g. [read()](@ref virtmem::BaseVAlloc::read) and
[write()](@ref virtmem::BaseVAlloc::write)). Although dealing with raw memory might be
slightly more efficient performance wise, this approach is not recommended as
it is fairly cumbersome to do so.

The second approach is to use _virtual pointer wrappers_. These template
classes were designed to make virtual memory access as close as 'regular'
memory access as possible. Here is an example:

~~~{.cpp}
// define virtual pointer linked to SD fat memory
virtmem::VPtr<int, virtmem::SDVAlloc> vptr;
vptr = valloc.alloc<int>(); // allocate memory to store integer (size automatically deduced from type)
*vptr = 4;
~~~

In this example we defined a virtual pointer to an `int`, which is linked to the
[SD allocator](@ref virtmem::SDVAlloc). An alternative syntax to define a virtual pointer is
through virtmem::VAlloc::TVPtr:

~~~{.cpp}
virtmem::SDVAlloc::TVPtr<int>::type vptr;
~~~

If your platform [supports C++11](@ref aCPP11Support), this can shortened further:

~~~{.cpp}
virtmem::SDVAlloc::VPtr<int> vptr; // Same as above but shorter, C++11 only!
~~~

Staying on C++11 support, using `auto` further reduces the syntax quite a bit:

~~~{.cpp}
auto vptr = valloc.alloc<int>(); // Automatically deduce vptr type from alloc call, C++11 only!
~~~

Memory allocation is done through the [alloc()](@ref virtmem::VAlloc::alloc)
function. In the above example no arguments were passed to [alloc()](@ref virtmem::VAlloc::alloc),
which means that `alloc` will automatically deduce the size required for the pointer
type (`int`). If you want to allocate a different size (for instance to use the data
as an array) then the number of bytes should be specified as the first argument to `alloc`:

~~~{.cpp}
vptr = valloc.alloc<int>(1000 * sizeof(int)); // allocate memory to store array of 1000 integers
vptr[500] = 1337;
~~~

Besides all standard types (char, int, short, etc.), virtual pointers can also
work with custom types (structs/classes):

~~~{.cpp}
struct MyStruct { int x, y; };
// ...
virtmem::VPtr<MyStruct, virtmem::SDVAlloc> vptr = valloc.alloc<MyStruct>();
ms->x = 5;
ms->y = 15;
~~~
Note that there are a few imitations when using structs (or classes) with `virtmem`. For details: see virtmem::VPtr.

Finally, to free memory the [free()](@ref virtmem::VAlloc::free) function can be used:

~~~{.cpp}
valloc.free(vptr); // memory size is automatically deduced
~~~

@sa
* Allocator base classes: virtmem::VAlloc and virtmem::BaseVAlloc
* Allocator classes: virtmem::SerialVAllocP, virtmem::SDVAllocP, virtmem::SPIRAMVAllocP and virtmem::MultiSPIRAMVAllocP
* Pointer classes: virtmem::VPtr and virtmem::BaseVPtr
* <a href="examples.html">More examples</a>
* the rest of this manual on advanced usage

# Advanced {#advanced}

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
[locks created when accessing data in structures/classes](@ref aAccess).

The default size and amount of memory pages is dependent upon the MCU platform
and can be customized as described in @ref aConfigAlloc.

### Using virtual data locks {#alUsing}
To create a lock to virtual memory the virtmem::VPtrLock class is used:

~~~{.cpp}
typedef virtmem::VPtr<char, virtmem::SDValloc> virtCharPtr; // shortcut
virtCharPtr vptr = valloc.alloc<char>(100); // allocate some virtual memory
virtmem::VPtrLock<virtCharPtr> lock = virtmem::makeVirtPtrLock(vptr, 100, false);
memset(*lock, 10, 100); // set all bytes to '10'
~~~

The virtmem::makeVirtPtrLock function is used to create a lock. The last
parameter to this function (optional, by default `false`) tells whether the
locked data should be treated as read-only: if set to `false` the data will be
written back to the virtual memory (even if unchanged) after the lock has been released.
If you know the data will not be changed (or you don't care about changes), then it is
more efficient to pass `true` instead.

Accessing locked data is simply done by dereferencing the lock variable (i.e. `*lock`).

Sometimes it is not possible to completely lock the memory region that was
requested. For instance, there may not be sufficient space available to lock
the complete data to a memory page or there will be overlap with another locked
memory region. For this reason, it is **important to always check the _actual
size_ that was locked**. After a lock has been made, the effective size of the
locked region can be requested by the virtmem::VPtrLock::getLockSize()
function. Because it is rather unpredictable whether the requested data can be
locked in one go, it is best create a loop that iteratively creates locks until
all bytes have been dealt with:

~~~{.cpp}
typedef virtmem::VPtr<char, virtmem::SDValloc> virtCharPtr; // shortcut
const int size = 10000;
int sizeleft = size;

virtCharPtr vptr = valloc.alloc(size); // allocate a large block of virtual memory
virtCharPtr p = vptr;

while (sizeleft)
{
    // create a lock to (a part) of the buffer
    virtmem::VPtrLock<virtCharPtr> lock = virtmem::makeVirtPtrLock(p, sizeleft, false);

    const int lockedsize = lock.getLockSize(); // get the actual size of the memory block that was locked
    memset(*l, 10, lockedsize); // set bytes of locked (sub region) to 10

    p += lockedsize; // increase pointer to next block to lock
    sizeleft -= lockedsize; // decrease number of bytes to still lock
}

~~~
Note that a `memset` overload is provided by `virtmem` which works with virtual pointers.

After you are finished working with a virtual memory lock it has to be
released. This can be done manually with the virtmem::VPtrLock::unlock
function. However, the virtmem::VPtrLock destructor will call this function
automatically (the class follows the
[RAII principle](https://en.wikipedia.org/wiki/Resource_Acquisition_Is_Initialization)).
This explains why calling `unlock()` in the above example was not necessary, as
the destructor will call it automatically when the `lock` variable goes out of
scope at the end of every iteration.

## Accessing data in virtual memory {#aAccess}

@note This section is mostly theoretical. If you are skimming this manual (or
lazy), you can skip this section.

Whenever virtual data is accessed, `virtmem` first has to make sure that this
data resides in regular RAM (i.e. a memory page). In addition, if the data is
changed, `virtmem` has to flag this data as 'dirty' so that it will be
synchronized during the next page swap. To achieve these tasks, `virtmem`
returns a _proxy class_ whenever a virtual pointer is dereferenced, instead of
returning the actual data. This proxy class (virtmem::VPtr::ValueWrapper)
acts as it is the actual data, and is mostly invisible to the user:

~~~{.cpp}
int x = *myIntVptr;
*myIntVptr = 55;
~~~

The first line of the above example demonstrates a read operation: in this case
the proxy class (returned by `*myIntVPtr`) will be converted to an int
(automatic type cast), during which it will return a copy the data from virtual
memory. The second line is a write operation: here the proxy class signals
`virtmem` that data is changed and needs to be synchronized during the next
page swap.

For accessing data members of structures (everything discussed here also
applies to classes) in virtual memory the situation is more complicated. When a
member is accessed through the `->` operator of of the virtual pointer, we
[must return an actual pointer to the
structure](http://stackoverflow.com/a/8782794). While it is possible to make
this data available through a memory page (as is done normally) in the `->`
overload function, synchronizing the data back is more tricky. For this reason,
another proxy class is used (virtmem::VPtr::MemberWrapper) which is
returned when the `->` operator is called. This proxy class has also its `->`
operator overloaded, and this overload returns the actual data. The lifetime of
this proxy class is important and matches that of the lifetime the data needs
to be available in a memory page (for more details, see [Stroustrup's general
wrapper paper](http://www.stroustrup.com/wrapper.pdf)). Following from this,
the proxy class will create a [data lock](@ref aLocking) to the data of the
structure during construction and release this lock during its destruction.

## Wrapping regular pointers {#aWrapping}

Sometimes it may be handy to assign a regular pointer to a virtual pointer:
~~~{.cpp}
typedef virtmem::VPtr<int, virtmem::SDVAlloc>::type virtIntPtr; // shortcut

void f(virtIntPtr p, int x)
{
    *p += x;
}

// ...

*vptr = 55; // vptr is a virtual pointer to int
*ptr = 66; // ptr is a regular pointer to int (i.e. int*)

f(vptr, 10);
f(ptr, 12); // ERROR! Function only accepts virtual pointers
~~~

In the above example we have a (nonsensical) function `f`, which only accepts
virtual pointers. Hence, the final line in this example will fail. Of course we
could overload this function and provide an implementation that supports
regular pointers. Alternatively, you can also 'wrap' the regular pointer inside
a virtual pointer:

~~~{.cpp}
// ...

*ptr = 66; // ptr is a regular pointer to int (i.e. int*)
virtIntPtr myvptr = myvptr.wrap(ptr); // 'wrap' ptr inside a virtual pointer

f(myvptr, 12); // Success!
~~~

When a regular pointer is wrapped inside a virtual pointer, it can be used as it
were a virtual pointer. If you want to obtain the original pointer then you can use the
[unwrap()](@ref virtmem::VPtr::unwrap) function:

~~~{.cpp}
int *myptr = vptr.unwrap();
~~~

Please note that calling `unwrap()` on a non-wrapped virtual pointer yields an
invalid pointer address. To avoid this, the
[isWrapped() function](@ref virtmem::BaseVPtr::isWrapped) can be used.

@note Wrapping regular pointers introduces a small overhead in usage of virtual
pointers and is therefore **disabled by default**. This feature can be enabled
in @ref config.h.

## Dealing with large data structures {#aLargeStructs}

Consider the following code:

~~~{.cpp}
using namespace virtmem;

struct MyStruct
{
    int x, y;
    char buffer[1024];
};

// ...

// allocate MyStruct in virtual memory
VPtr<MyStruct, SDVAlloc> sptr = valloc.alloc<MyStruct>();
sptr->buffer[512] = 'B'; // assign some random value
~~~

The size of MyStruct contains a large buffer and the total size exceeds 1 kilobyte. A good reason
to put it in virtual memory! However, when assignment occurs in the example above,
dereferencing the virtual pointer causes a copy of the whole data structure to a virtual page
([more info here](@ref aAccess)). This means that the memory page must be sufficient in size. One
option is to [configure the allocator](@ref aConfigAlloc) and make sure memory pages are big enough.
However, since this will increase RAM usage, this may not be an option.

For the example outlined above, another option may be to let MyStruct::buffer be a virtual pointer
to a buffer in virtual memory:
~~~{.cpp}
using namespace virtmem;

struct MyStruct
{
    int x, y;
    VPtr<char, SDVAlloc> buffer;
};

// ...

// allocate MyStruct in virtual memory
VPtr<MyStruct, SDVAlloc> sptr = valloc.alloc<MyStruct>();

// ... and allocate buffer
sptr->buffer = valloc.alloc<char>(1024);

sptr->buffer[512] = 'B'; // assign some random value
~~~

Since MyStruct only stores a virtual pointer, its _much_ smaller and can easily fit into a memory page.

## Multiple allocators {#aMultiAlloc}

While not more than one instance of a memory allocator _type_ should be
defined, it is possible to define different allocators in the same program:

~~~{.cpp}
virtmem::SDVAlloc fatAlloc;
virtmem::SPIRAMVAlloc spiRamAlloc;

virtmem::VPtr<int, virtmem::SDVAlloc> fatvptr;
virtmem::VPtr<int, virtmem::SPIRAMVAlloc> spiramvptr;

// ...

// copy a kilobyte of data from SPI ram to a SD fat virtual memory pool
virtmem::memcpy(fatvptr, spiramvptr, 1024);
~~~

## Configuring allocators {#aConfigAlloc}

The number and size of memory pages can be configured in config.h.
Alternatively, these settings can be passed as a template parameter to the
allocator. For more info, see the description about
virtmem::DefaultAllocProperties.

## Virtual pointers to `struct`/`class` data members {#aPointStructMem}

It might be necessary to obtain a pointer to a member of a structure (or class)
which resides in virtual memory. The way to obtain such a pointer with
'regular' memory is by using the address-of operator ('`@`'):

~~~{.cpp}
int *p = &mystruct->x;
~~~

You may be tempted to do the same when the structure is in virtual memory:

~~~{.cpp}
int *p = &vptr->x; // Spoiler: Do not do this!
~~~

However, this should **never** be done! The problem is that the above code will
set `p` to an address inside one of the virtual memory pages. This should be
considered as a temporary storage location and the contents can be changed
anytime.

To obtain a 'safe' pointer, one should use the [getMembrPtr() function](@ref virtmem::getMembrPtr):

~~~{.cpp}
struct myStruct { int x; };
virtmem::VPtr<int, virtmem::SDVAlloc> intvptr;

intvptr = virtmem::getMembrPtr(mystruct, &myStruct::x);
~~~

@sa virtmem::getMembrPtr

## Overloads of some common C library functions for virtual pointers {#aCoverloads}

Overloads of some common C functions for dealing with memory and strings are
provided by `virtmem`. They accept virtual pointers or a mix of virtual and
regular pointers as function arguments. Please note that they are defined in the
[virtmem namespace](@ref virtmem) like any other code from `virtmem`, hence, they will
not "polute" the global namespace unless you want to (i.e. by using the `using`
directive).

The following function overloads exist:
* `memcpy`
* `memset`
* `memcmp`
* `strncpy`
* `strcpy`
* `strncmp`
* `strcmp`
* `strlen`

@sa [Overview of all overloaded functions](@ref Coverloads).

## Typeless virtual pointers (analog to void*) {#aTypeless}

When working with regular pointers, a `void` pointer can be used to store
whatever pointer you like. With virtual pointers something similar can be
achieved by using the base class for virtual pointers, namely
virtmem::BaseVPtr:

~~~{.cpp}
typedef virtmem::VPtr<int, virtmem::SDVAlloc> virtIntPtr; // shortcut

int *intptr;
virtIntPtr intvptr;

// ...

// Store pointers in typeless pointer
void *voidptr = intptr;
virtmem::BaseVPtr basevptr = intvptr;

// ...

// Get it back
intptr = static_cast<int *>(voidptr);
intvptr = static_cast<virtIntPtr>(basevptr);
~~~

## Generalized NULL pointer {#aNILL}

When dealing with both regular and virtual pointers, it may be handy to use a single value that
can set both types to zero. For this virtmem::NILL can be used:
~~~{.cpp}
using namespace virtmem;

// ...

char *a = NILL;
VPtr<char, SDVAlloc> b = NILL;
~~~

Note that on platforms support C++11 you can simply use `nullptr` instead.

## Pointer conversion {#aPConv}

Similar to regular pointers, sometimes it may be necessary to convert from one pointer type to another.
For this a simple typecast works as expected:
~~~{.cpp}
using namespace virtmem;

// ...

char buffer[128];
VPtr<char, SDVAlloc> vbuffer = valloc.alloc<char>(128);

int *ibuf = (int *)buffer; // or reinterpret_cast<int *>(buffer);
VPtr<int, SDVAlloc> vibuf = (VPtr<int, SDValloc>)vbuffer; // or static_cast<VPtr<int, SDValloc> >(vbuffer);
~~~

## C++11 support {#aCPP11Support}

Some platforms such as the latest Arduino versions (>= 1.6.6) or Teensyduino support the fairly
recent C++11 standard. In `virtmem` you can take this to your advantage to shorten the syntax,
for instance by using [template aliasing](http://en.cppreference.com/w/cpp/language/type_alias):

~~~{.cpp}
template <typename T> using MyVPtr = virtmem::VPtr<T, virtmem::SDVAlloc>;
MyVPtr<int> intVPtr;
MyVPtr<char> charVPtr;
// etc
~~~

In fact, this feature is already used by allocator classes:
~~~{.cpp}
virtmem::SDVAlloc::VPtr<int> intVPtr;
~~~

The new `auto` keyword support means that we can further reduce the syntax quite a bit:
~~~{.cpp}
virtmem::SDVAlloc valloc;
//...
auto intVPtr = valloc.alloc<int>(); // automatically deduce correct type from allocation call
~~~

Another, small feature with C++11 support, is that `nullptr` can be used to assign a zero address to
a virtual pointer.

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
* Supports SPI RAM (23LC series from Microchip), SD cards and RAM from a computer connected through serial.
* Easy C++ interface that closely resembles regular data access.
* New memory interfaces can be added easily
* Easily ported to other plaforms (x86 port exists for debugging)

# Manual
The manual [can be found here](http://rhelmus.github.io/virtmem/index.html).

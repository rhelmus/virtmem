#-------------------------------------------------
#
# Project created by QtCreator 2014-12-08T21:07:02
#
#-------------------------------------------------

QT       -= core gui

TARGET = virtmem
TEMPLATE = lib
CONFIG += staticlib

SOURCES += \
    base_alloc.cpp \
    utils.cpp \

HEADERS += \
    virtmem.h \
    internal/utils.h \
    internal/base_alloc.h \
    config/config.h \
    alloc/stdio_alloc.h \
    internal/alloc.h \
    alloc/spiram_alloc.h \
    alloc/static_alloc.h \
    alloc/sd_alloc.h \
    internal/vptr_utils.h \
    internal/vptr.h \
    internal/base_vptr.h \
    internal/vptr_utils.hpp \
    alloc/serial_alloc.h \
    internal/serial_utils.h \
    internal/serial_utils.hpp
unix {
    target.path = /usr/lib
    INSTALLS += target
}

OTHER_FILES += \
    ../examples/benchmark/benchmark.ino \
    ../examples/benchmark/benchmark.h \
    ../examples/serial_simple/serial_simple.ino \
    ../examples/serial_input/serial_input.ino \
    ../examples/spiram_simple/spiram_simple.ino \
    ../examples/multispiram_simple/multispiram_simple.ino \
    ../examples/sd_simple/sd_simple.ino \
    ../examples/alloc_properties/alloc_properties.ino \
    ../examples/locking/locking.ino

QMAKE_CXXFLAGS_RELEASE += -Os
#QMAKE_CXXFLAGS_DEBUG += -Og
QMAKE_CXXFLAGS +=  -std=gnu++11 -ffunction-sections -fdata-sections
QMAKE_LFLAGS += -Wl,--gc-sections

OTHER_FILES += \
    arduinofy.sh \
    ../extras/serial_host.py \
    ../extras/serialiohandler.py

DISTFILES += \
    ../../README.md \
    ../TODO.txt \
    ../../doc/manual.md \
    ../../doc/Doxyfile

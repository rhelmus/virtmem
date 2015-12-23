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
    utils.h \
    base_alloc.h \
    config.h \
    stdio_alloc.h \
    alloc.h \
    spiram_alloc.h \
    static_alloc.h \
    sd_alloc.h \
    vptr_utils.h \
    vptr.h \
    base_vptr.h \
    vptr_utils.hpp \
    serial_alloc.h \
    serial_utils.h \
    serial_utils.hpp
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
    ../examples/locking/locking.ino \
    ../Doxyfile \
    ../groups.dox

QMAKE_CXXFLAGS_RELEASE += -Os
#QMAKE_CXXFLAGS_DEBUG += -Og
QMAKE_CXXFLAGS +=  -std=gnu++11 -ffunction-sections -fdata-sections
QMAKE_LFLAGS += -Wl,--gc-sections

OTHER_FILES += \
    arduinofy.sh \
    ../script/serial_host.py \
    ../script/serialiohandler.py

DISTFILES += \
    ../README.md \
    TODO.txt \
    ../doc/manual.md \
    ../doc/Doxyfile

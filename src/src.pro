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

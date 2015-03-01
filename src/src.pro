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
    wrapper.cpp \
    base_alloc.cpp \
    utils.cpp \
    serram_utils.cpp

HEADERS += \
    virtmem.h \
    base_wrapper.h \
    wrapper.h \
    utils.h \
    base_alloc.h \
    config.h \
    utils.hpp \
    stdio_alloc.h \
    sdfatlib_alloc.h \
    alloc.h \
    wrapper_utils.h \
    spiram_alloc.h \
    static_alloc.h \
    serram_utils.h \
    serram_alloc.h
unix {
    target.path = /usr/lib
    INSTALLS += target
}

QMAKE_CXXFLAGS_RELEASE += -Os
#QMAKE_CXXFLAGS_DEBUG += -Og
QMAKE_CXXFLAGS +=  -std=gnu++11 -ffunction-sections -fdata-sections
QMAKE_LFLAGS += -Wl,--gc-sections

OTHER_FILES += \
    arduinofy.sh \
    ../script/serial_host.py \
    ../script/serialiohandler.py

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
    utils.cpp

HEADERS += \
    virtmem.h \
    base_wrapper.h \
    wrapper.h \
    utils.h \
    alloc.h \
    base_alloc.h \
    stdioalloc.h \
    config.h \
    utils.hpp
unix {
    target.path = /usr/lib
    INSTALLS += target
}

QMAKE_CXXFLAGS += -g3 -m32 -std=gnu++11

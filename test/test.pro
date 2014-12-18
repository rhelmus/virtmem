TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    test_alloc.cpp \
    test_wrapper.cpp \
    test_utils.cpp

HEADERS += \
    test.h

INCLUDEPATH += .

unix:!macx: LIBS += -L$$PWD/../src/ -lvirtmem

INCLUDEPATH += $$PWD/../src
DEPENDPATH += $$PWD/../src

unix:!macx: PRE_TARGETDEPS += $$PWD/../src/libvirtmem.a

LIBS += -lgtest -lgtest_main
DEFINES += __STDC_FORMAT_MACROS
QMAKE_CXXFLAGS += -std=gnu++11 -Os

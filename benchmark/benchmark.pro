TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    benchmark.cpp

include(deployment.pri)
qtcAddDeployment()

INCLUDEPATH += $$PWD/../virtmem/src .
DEPENDPATH += $$PWD/../virtmem/src

LIBS += -L$$PWD/../virtmem/src/ -lvirtmem
unix:!macx: PRE_TARGETDEPS += $$PWD/../virtmem/src/libvirtmem.a

QMAKE_CXXFLAGS +=  -std=gnu++11

TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    benchmark.cpp

include(deployment.pri)
qtcAddDeployment()

INCLUDEPATH += $$PWD/../src .
DEPENDPATH += $$PWD/../src

LIBS += -L$$PWD/../src/ -lvirtmem
unix:!macx: PRE_TARGETDEPS += $$PWD/../src/libvirtmem.a

QMAKE_CXXFLAGS +=  -std=gnu++11

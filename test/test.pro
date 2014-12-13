TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += test.cpp

HEADERS +=

INCLUDEPATH += .

unix:!macx: LIBS += -L$$PWD/../src/ -lvirtmem

INCLUDEPATH += $$PWD/../src
DEPENDPATH += $$PWD/../src

unix:!macx: PRE_TARGETDEPS += $$PWD/../src/libvirtmem.a

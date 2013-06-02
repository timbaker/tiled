include($$top_srcdir/tiled.pri)
include($$top_srcdir/src/libtiled/libtiled.pri)

TEMPLATE = lib
CONFIG += static
CONFIG -= app_bundle
TARGET = worlded

DESTDIR = $$top_builddir/lib

DEFINES += QT_NO_CAST_FROM_ASCII \
    QT_NO_CAST_TO_ASCII

DEFINES += ZOMBOID WORLDED

INCLUDEPATH += $$top_srcdir/src/tiled

SOURCES += \
    properties.cpp \
    road.cpp \
    world.cpp \
    worldcell.cpp \
    worldcellobject.cpp \
    worldreader.cpp \
    worldedmgr.cpp
    
HEADERS +=  \
    properties.h \
    road.h \
    world.h \
    worldcell.h \
    worldreader.h \
    worldedmgr.h

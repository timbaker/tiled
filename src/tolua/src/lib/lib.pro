include(../../../../tiled.pri)
include(../../../lua/lua.pri)

TEMPLATE = lib
CONFIG += static
CONFIG -= app_bundle
CONFIG -= qt

TARGET = tolua

DESTDIR = ../../../../lib

INCLUDEPATH += \
    ../../include

HEADERS += \
    tolua_event.h \
    ../../include/tolua.h

SOURCES += \
    tolua_to.c \
    tolua_push.c \
    tolua_map.c \
    tolua_is.c \
    tolua_event.c

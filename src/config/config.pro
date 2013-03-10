include(../../tiled.pri)

TEMPLATE = app
TARGET = config
target.path = $${PREFIX}/bin
INSTALLS += target
TEMPLATE = app
win32 {
    DESTDIR = ../..
} else {
    DESTDIR = ../../bin
}

macx {
    QMAKE_LIBDIR_FLAGS += -L$$OUT_PWD/../../bin/TileZed.app/Contents/Frameworks
} else:win32 {
    LIBS += -L$$OUT_PWD/../../lib
} else {
    QMAKE_LIBDIR_FLAGS += -L$$OUT_PWD/../../lib
}

SOURCES += main.cpp \
    configdialog.cpp

HEADERS += \
    configdialog.h

FORMS += \
    configdialog.ui


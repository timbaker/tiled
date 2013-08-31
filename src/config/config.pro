include(../../tiled.pri)

TEMPLATE = app
TARGET = config
isEmpty(INSTALL_ONLY_BUILD) {
    target.path = $${PREFIX}/bin
    INSTALLS += target
}
TEMPLATE = app
win32 {
    DESTDIR = ../..
} else {
    DESTDIR = ../../bin
}

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += gui widgets
}

DEFINES += QT_NO_CAST_FROM_ASCII \
    QT_NO_CAST_TO_ASCII

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


include(../../tiled.pri)
include(../libtiled/libtiled.pri)

TEMPLATE = app
TARGET = tmxviewer
isEmpty(INSTALL_ONLY_BUILD) {
    target.path = $${PREFIX}/bin
    INSTALLS += target
}
TEMPLATE = app

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

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

# Make sure the executable can find libtiled
!win32:!macx {
    QMAKE_RPATHDIR += \$\$ORIGIN/../lib

    # It is not possible to use ORIGIN in QMAKE_RPATHDIR, so a bit manually
    QMAKE_LFLAGS += -Wl,-z,origin \'-Wl,-rpath,$$join(QMAKE_RPATHDIR, ":")\'
    QMAKE_RPATHDIR =
}

SOURCES += main.cpp \
         tmxviewer.cpp

HEADERS += tmxviewer.h

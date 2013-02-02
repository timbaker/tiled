include(../../tiled.pri)

TEMPLATE = lib
TARGET = qtlockedfile
target.path = $${LIBDIR}
INSTALLS += target
macx {
    DESTDIR = ../../bin/TileZed.app/Contents/Frameworks
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@executable_path/../Frameworks/
} else {
    DESTDIR = ../../lib
}
DLLDESTDIR = ../..

DEFINES += QT_NO_CAST_FROM_ASCII \
    QT_NO_CAST_TO_ASCII

HEADERS += qtlockedfile.h
SOURCES += qtlockedfile.cpp

unix:SOURCES += qtlockedfile_unix.cpp
win32:SOURCES += qtlockedfile_win.cpp

win32:contains(TEMPLATE, lib):contains(CONFIG, shared) {
    DEFINES += QT_QTLOCKEDFILE_EXPORT=__declspec(dllexport)
}

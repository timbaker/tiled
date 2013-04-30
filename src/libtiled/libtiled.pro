include(../../tiled.pri)
include(../qtlockedfile/qtlockedfile.pri)

TEMPLATE = lib
TARGET = tiled
target.path = $${LIBDIR}
INSTALLS += target
macx {
    DESTDIR = ../../bin/TileZed.app/Contents/Frameworks
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@executable_path/../Frameworks/
} else {
    DESTDIR = ../../lib
}
DLLDESTDIR = ../..

win32 {
    # It is enough to include zlib, since the symbols are available in Qt
    INCLUDEPATH += ../zlib
} else {
    # On other platforms it is necessary to link to zlib explicitly
    LIBS += -lz
}

DEFINES += QT_NO_CAST_FROM_ASCII \
    QT_NO_CAST_TO_ASCII
DEFINES += TILED_LIBRARY
DEFINES += ZOMBOID
contains(QT_CONFIG, reduce_exports): CONFIG += hide_symbols
#OBJECTS_DIR = .obj
SOURCES += compression.cpp \
    imagelayer.cpp \
    isometricrenderer.cpp \
    layer.cpp \
    map.cpp \
    mapobject.cpp \
    mapreader.cpp \
    maprenderer.cpp \
    mapwriter.cpp \
    objectgroup.cpp \
    orthogonalrenderer.cpp \
    properties.cpp \
    staggeredrenderer.cpp \
    tilelayer.cpp \
    tileset.cpp \
    gidmapper.cpp \
    zlevelrenderer.cpp \
    ztilelayergroup.cpp \
    luatiled.cpp
HEADERS += compression.h \
    imagelayer.h \
    isometricrenderer.h \
    layer.h \
    map.h \
    mapobject.h \
    mapreader.h \
    maprenderer.h \
    mapwriter.h \
    object.h \
    objectgroup.h \
    orthogonalrenderer.h \
    properties.h \
    staggeredrenderer.h \
    tile.h \
    tiled_global.h \
    tilelayer.h \
    tileset.h \
    gidmapper.h \
    zlevelrenderer.h \
    ztilelayergroup.h \
    luatiled.h \
    libtiled.pkg
macx {
    contains(QT_CONFIG, ppc):CONFIG += x86 \
        ppc
}

#LIBS += -L../../lib
include(../tolua/src/lib/tolua.pri)
include(../lua/lua.pri)
TOLUA_PKGNAME = libtiled
TOLUA_PKG = libtiled.pkg
include(../tolua/src/bin/tolua.pri)


INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
LIBS += -L$$top_builddir/lib -lworlded
win32-msvc*:PRE_TARGETDEPS += $$top_builddir/lib/worlded.lib
*g++*:PRE_TARGETDEPS += $$top_builddir/lib/libworlded.a

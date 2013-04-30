INCLUDEPATH += $$top_srcdir/src/tolua/include
#DEPENDPATH += $$PWD
LIBS += -L$$top_builddir/lib -ltolua
PRE_TARGETDEPS += $$top_builddir/lib/tolua.lib

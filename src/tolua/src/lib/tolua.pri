INCLUDEPATH += $$PWD/../../include
DEPENDPATH += $$PWD/../../include
LIBS += -L$$top_builddir/lib -ltolua
PRE_TARGETDEPS += $$top_builddir/lib/tolua.lib

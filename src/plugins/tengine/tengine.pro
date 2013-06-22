include(../plugin.pri)

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

DEFINES += TENGINE_LIBRARY

SOURCES += tengineplugin.cpp
HEADERS += tengineplugin.h\
        tengine_global.h

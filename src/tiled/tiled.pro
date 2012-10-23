include(../../tiled.pri)
include(../libtiled/libtiled.pri)

TEMPLATE = app
TARGET = TileZed
target.path = $${PREFIX}/bin
INSTALLS += target
win32 {
    DESTDIR = ../..
} else {
    DESTDIR = ../../bin
}
contains(QT_CONFIG, opengl): QT += opengl

DEFINES += QT_NO_CAST_FROM_ASCII \
    QT_NO_CAST_TO_ASCII
DEFINES += ZOMBOID

macx {
    QMAKE_LIBDIR_FLAGS += -L$$OUT_PWD/../../bin/TileZed.app/Contents/Frameworks
    LIBS += -framework Foundation
} else:win32 {
    LIBS += -L$$OUT_PWD/../../lib
} else {
    QMAKE_LIBDIR_FLAGS += -L$$OUT_PWD/../../lib
}

# Make sure the Tiled executable can find libtiled
!win32:!macx {
    QMAKE_RPATHDIR += \$\$ORIGIN/../lib

    # It is not possible to use ORIGIN in QMAKE_RPATHDIR, so a bit manually
    QMAKE_LFLAGS += -Wl,-z,origin \'-Wl,-rpath,$$join(QMAKE_RPATHDIR, ":")\'
    QMAKE_RPATHDIR =
}

MOC_DIR = .moc
UI_DIR = .uic
RCC_DIR = .rcc
OBJECTS_DIR = .obj

SOURCES += aboutdialog.cpp \
    abstractobjecttool.cpp \
    abstracttiletool.cpp \
    abstracttool.cpp \
    addremovelayer.cpp \
    addremovemapobject.cpp \
    addremovetileset.cpp \
    automapper.cpp \
    automapperwrapper.cpp \
    automappingmanager.cpp \
    automappingutils.cpp  \
    brushitem.cpp \
    bucketfilltool.cpp \
    changemapobject.cpp \
    changeimagelayerproperties.cpp \
    changeobjectgroupproperties.cpp \
    changepolygon.cpp \
    changeproperties.cpp \
    changetileselection.cpp \
    clipboardmanager.cpp \
    colorbutton.cpp \
    commandbutton.cpp \
    command.cpp \
    commanddatamodel.cpp \
    commanddialog.cpp \
    commandlineparser.cpp \
    createobjecttool.cpp \
    documentmanager.cpp \
    editpolygontool.cpp \
    eraser.cpp \
    erasetiles.cpp \
    filesystemwatcher.cpp \
    filltiles.cpp \
    imagelayeritem.cpp \
    imagelayerpropertiesdialog.cpp \
    languagemanager.cpp \
    layerdock.cpp \
    layermodel.cpp \
    main.cpp \
    mainwindow.cpp \
    mapdocumentactionhandler.cpp \
    mapdocument.cpp \
    mapobjectitem.cpp \
    mapobjectmodel.cpp \
    mapscene.cpp \
    mapsdock.cpp \
    mapview.cpp \
    movelayer.cpp \
    movemapobject.cpp \
    movemapobjecttogroup.cpp \
    movetileset.cpp \
    newmapdialog.cpp \
    newtilesetdialog.cpp \
    objectgroupitem.cpp \
    objectgrouppropertiesdialog.cpp \
    objectpropertiesdialog.cpp \
    objectsdock.cpp \
    objectselectiontool.cpp \
    objecttypes.cpp \
    objecttypesmodel.cpp \
    offsetlayer.cpp \
    offsetmapdialog.cpp \
    painttilelayer.cpp \
    pluginmanager.cpp \
    preferences.cpp \
    preferencesdialog.cpp \
    propertiesdialog.cpp \
    propertiesmodel.cpp \
    propertiesview.cpp \
    quickstampmanager.cpp \
    renamelayer.cpp \
    resizedialog.cpp \
    resizehelper.cpp \
    resizelayer.cpp \
    resizemap.cpp \
    resizemapobject.cpp \
    saveasimagedialog.cpp \
    selectionrectangle.cpp \
    stampbrush.cpp \
    tiledapplication.cpp \
    tilelayeritem.cpp \
    tilepainter.cpp \
    tileselectionitem.cpp \
    tileselectiontool.cpp \
    tilesetdock.cpp \
    tilesetmanager.cpp \
    tilesetmodel.cpp \
    tilesetview.cpp \
    tmxmapreader.cpp \
    tmxmapwriter.cpp \
    toolmanager.cpp \
    undodock.cpp \
    utils.cpp \
    zoomable.cpp \
    zgriditem.cpp \
    zlevelsdock.cpp \
    zlevelsmodel.cpp \
    zlotmanager.cpp \
    ZomboidScene.cpp \
    zprogress.cpp \
    ztilelayergroupitem.cpp \
    ztilesetthumbmodel.cpp \
    ztilesetthumbview.cpp \
    mapcomposite.cpp \
    mapmanager.cpp \
    mapimagemanager.cpp \
    minimap.cpp \
    convertorientationdialog.cpp \
    converttolotdialog.cpp \
    BuildingEditor/buildingeditorwindow.cpp \
    BuildingEditor/simplefile.cpp \
    BuildingEditor/buildingtools.cpp \
    BuildingEditor/FloorEditor.cpp \
    BuildingEditor/buildingdocument.cpp \
    BuildingEditor/building.cpp \
    BuildingEditor/buildingfloor.cpp \
    BuildingEditor/buildingundoredo.cpp \
    BuildingEditor/buildingpreviewwindow.cpp \
    BuildingEditor/mixedtilesetview.cpp \
    BuildingEditor/newbuildingdialog.cpp \
    BuildingEditor/buildingpreferencesdialog.cpp \
    BuildingEditor/buildingobjects.cpp \
    BuildingEditor/buildingtemplates.cpp \
    BuildingEditor/buildingtemplatesdialog.cpp \
    BuildingEditor/choosebuildingtiledialog.cpp \
    BuildingEditor/roomsdialog.cpp \
    BuildingEditor/buildingtilesdialog.cpp \
    BuildingEditor/buildingtiles.cpp \
    BuildingEditor/templatefrombuildingdialog.cpp

HEADERS += aboutdialog.h \
    abstractobjecttool.h \
    abstracttiletool.h \
    abstracttool.h \
    addremovelayer.h \
    addremovemapobject.h \
    addremovetileset.h \
    automapper.h \
    automapperwrapper.h \
    automappingmanager.h \
    automappingutils.h \
    brushitem.h \
    bucketfilltool.h \
    changemapobject.h \
    changeimagelayerproperties.h\
    changeobjectgroupproperties.h \
    changepolygon.h \
    changeproperties.h \
    changetileselection.h \
    clipboardmanager.h \
    colorbutton.h \
    commandbutton.h \
    commanddatamodel.h \
    commanddialog.h \
    command.h \
    commandlineparser.h \
    createobjecttool.h \
    documentmanager.h \
    editpolygontool.h \
    eraser.h \
    erasetiles.h \
    filesystemwatcher.h \
    filltiles.h \
    imagelayeritem.h \
    imagelayerpropertiesdialog.h \
    languagemanager.h \
    layerdock.h \
    layermodel.h \
    macsupport.h \
    mainwindow.h \
    mapdocumentactionhandler.h \
    mapdocument.h \
    mapobjectitem.h \
    mapobjectmodel.h \
    mapreaderinterface.h \
    mapscene.h \
    mapsdock.h \
    mapview.h \
    mapwriterinterface.h \
    movelayer.h \
    movemapobject.h \
    movemapobjecttogroup.h \
    movetileset.h \
    newmapdialog.h \
    newtilesetdialog.h \
    objectgroupitem.h \
    objectgrouppropertiesdialog.h \
    objectpropertiesdialog.h \
    objectsdock.h \
    objectselectiontool.h \
    objecttypes.h \
    objecttypesmodel.h \
    offsetlayer.h \
    offsetmapdialog.h \
    painttilelayer.h \
    pluginmanager.h \
    preferencesdialog.h \
    preferences.h \
    propertiesdialog.h \
    propertiesmodel.h \
    propertiesview.h \
    quickstampmanager.h \
    rangeset.h \
    renamelayer.h \
    resizedialog.h \
    resizehelper.h \
    resizelayer.h \
    resizemap.h \
    resizemapobject.h \
    saveasimagedialog.h \
    selectionrectangle.h \
    stampbrush.h \
    tiledapplication.h \
    tilelayeritem.h \
    tilepainter.h \
    tileselectionitem.h \
    tileselectiontool.h \
    tilesetdock.h \
    tilesetmanager.h \
    tilesetmodel.h \
    tilesetview.h \
    tmxmapreader.h \
    tmxmapwriter.h \
    toolmanager.h \
    undocommands.h \
    undodock.h \
    utils.h \
    zoomable.h \
    ZomboidScene.h \
    mapcomposite.h \
    mapmanager.h \
    mapimagemanager.h \
    zlevelsdock.h \
    zlevelsmodel.h \
    zlotmanager.h \
    ztilesetthumbmodel.h \
    ztilesetthumbview.h \
    zgriditem.h \
    zprogress.h \
    minimap.h \
    convertorientationdialog.h \
    converttolotdialog.h \
    BuildingEditor/buildingeditorwindow.h \
    BuildingEditor/simplefile.h \
    BuildingEditor/buildingtools.h \
    BuildingEditor/FloorEditor.h \
    BuildingEditor/buildingdocument.h \
    BuildingEditor/building.h \
    BuildingEditor/buildingfloor.h \
    BuildingEditor/buildingundoredo.h \
    BuildingEditor/buildingpreviewwindow.h \
    BuildingEditor/mixedtilesetview.h \
    BuildingEditor/newbuildingdialog.h \
    BuildingEditor/buildingpreferencesdialog.h \
    BuildingEditor/buildingobjects.h \
    BuildingEditor/buildingtemplates.h \
    BuildingEditor/buildingtemplatesdialog.h \
    BuildingEditor/choosebuildingtiledialog.h \
    BuildingEditor/roomsdialog.h \
    BuildingEditor/buildingtilesdialog.h \
    BuildingEditor/buildingtiles.h \
    BuildingEditor/templatefrombuildingdialog.h

macx {
    OBJECTIVE_SOURCES += macsupport.mm
}

FORMS += aboutdialog.ui \
    commanddialog.ui \
    mainwindow.ui \
    newmapdialog.ui \
    newtilesetdialog.ui \
    objectpropertiesdialog.ui \
    offsetmapdialog.ui \
    preferencesdialog.ui \
    propertiesdialog.ui \
    resizedialog.ui \
    saveasimagedialog.ui\
    newimagelayerdialog.ui \
    convertorientationdialog.ui \
    converttolotdialog.ui \
    BuildingEditor/buildingeditorwindow.ui \
    BuildingEditor/buildingpreviewwindow.ui \
    BuildingEditor/newbuildingdialog.ui \
    BuildingEditor/buildingpreferencesdialog.ui \
    BuildingEditor/buildingtemplatesdialog.ui \
    BuildingEditor/choosebuildingtiledialog.ui \
    BuildingEditor/roomsdialog.ui \
    BuildingEditor/buildingtilesdialog.ui \
    BuildingEditor/templatefrombuildingdialog.ui

RESOURCES += tiled.qrc \
    BuildingEditor/buildingeditor.qrc
macx {
    TARGET = TileZed
    QMAKE_INFO_PLIST = Info.plist
    ICON = images/tiled-icon-mac.icns
}
win32 {
    RC_FILE = tiled.rc
}
win32:INCLUDEPATH += .
contains(CONFIG, static) {
    DEFINES += STATIC_BUILD
    QTPLUGIN += qgif \
        qjpeg \
        qtiff
}

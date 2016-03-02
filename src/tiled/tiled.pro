include(../../tiled.pri)
include(../libtiled/libtiled.pri)
include(../qtsingleapplication/qtsingleapplication.pri)
include(../qtlockedfile/qtlockedfile.pri)
include(../worlded/worlded.pri)

QMAKE_CFLAGS_RELEASE += -Zi
QMAKE_CXXFLAGS_RELEASE += -Zi
QMAKE_LFLAGS_RELEASE += /DEBUG /OPT:REF

TEMPLATE = app
TARGET = TileZed
isEmpty(INSTALL_ONLY_BUILD) {
    target.path = $${PREFIX}/bin
    INSTALLS += target
}
win32 {
    DESTDIR = ../..
} else {
    DESTDIR = ../../bin
}

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
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

#MOC_DIR = .moc
#UI_DIR = .uic
#RCC_DIR = .rcc
#OBJECTS_DIR = .obj

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
    mapcomposite.cpp \
    mapmanager.cpp \
    mapimagemanager.cpp \
    minimap.cpp \
    convertorientationdialog.cpp \
    converttolotdialog.cpp \
    BuildingEditor/simplefile.cpp \
    BuildingEditor/buildingtools.cpp \
    BuildingEditor/buildingdocument.cpp \
    BuildingEditor/building.cpp \
    BuildingEditor/buildingfloor.cpp \
    BuildingEditor/buildingundoredo.cpp \
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
    BuildingEditor/templatefrombuildingdialog.cpp \
    BuildingEditor/buildingwriter.cpp \
    BuildingEditor/buildingreader.cpp \
    BuildingEditor/resizebuildingdialog.cpp \
    BuildingEditor/furnitureview.cpp \
    BuildingEditor/furnituregroups.cpp \
    BuildingEditor/buildingpreferences.cpp \
    BuildingEditor/buildingtmx.cpp \
    BuildingEditor/tilecategoryview.cpp \
    BuildingEditor/listofstringsdialog.cpp \
    tilemetainfodialog.cpp \
    tilemetainfomgr.cpp \
    BuildingEditor/horizontallinedelegate.cpp \
    BuildingEditor/buildingfloorsdialog.cpp \
    BuildingEditor/buildingtiletools.cpp \
    BuildingEditor/buildingmap.cpp \
    BuildingEditor/buildingfurnituredock.cpp \
    BuildingEditor/buildingtilesetdock.cpp \
    BuildingEditor/buildinglayersdock.cpp \
    BuildingEditor/buildingeditorwindow.cpp \
    tiledefdialog.cpp \
    tiledeffile.cpp \
    addtilesetsdialog.cpp \
    BuildingEditor/buildingorthoview.cpp \
    BuildingEditor/buildingisoview.cpp \
    BuildingEditor/choosetemplatesdialog.cpp \
    threads.cpp \
    BuildingEditor/buildingtileentryview.cpp \
    bmptool.cpp \
    bmpblender.cpp \
    bmptooldialog.cpp \
    bmpselectionitem.cpp \
    BuildingEditor/buildingpropertiesdialog.cpp \
    roomdefecator.cpp \
    tilelayerspanel.cpp \
    roomdeftool.cpp \
    roomdefnamedialog.cpp \
    bmpruleview.cpp \
    luatiled.cpp \
    luaconsole.cpp \
    worldeddock.cpp \
    worldlottool.cpp \
    BuildingEditor/buildingdocumentmgr.cpp \
    BuildingEditor/categorydock.cpp \
    BuildingEditor/imode.cpp \
    BuildingEditor/objecteditmode.cpp \
    BuildingEditor/tileeditmode.cpp \
    BuildingEditor/editmodestatusbar.cpp \
    BuildingEditor/embeddedmainwindow.cpp \
    BuildingEditor/fancytabwidget.cpp \
    BuildingEditor/utils/stylehelper.cpp \
    BuildingEditor/utils/styledbar.cpp \
    BuildingEditor/welcomemode.cpp \
    BuildingEditor/buildingroomdef.cpp \
    picktiletool.cpp \
    mapbuildings.cpp \
    bmpblendview.cpp \
    luamapsdialog.cpp \
    luaworlddialog.cpp \
    edgetool.cpp \
    edgetooldialog.cpp \
    curbtool.cpp \
    curbtooldialog.cpp \
    fencetool.cpp \
    fencetooldialog.cpp \
    luatiletool.cpp \
    luatooldialog.cpp \
    luatooloptions.cpp \
    virtualtilesetdialog.cpp \
    virtualtilesetview.cpp \
    virtualtileset.cpp \
    undoredobuttons.cpp \
    addvirtualtilesetdialog.cpp \
    tileshapeeditor.cpp \
    textureunpacker.cpp \
    texturemanager.cpp \
    addtexturesdialog.cpp \
    enflatulatordialog.cpp \
    texturepropertiesdialog.cpp \
    tileshapegroupdialog.cpp \
    tileshapegrouppropertiesdialog.cpp \
    tileshapepropertiesdialog.cpp \
    pixelbuffer.cpp \
    vtsprefsdialog.cpp \
    packviewer.cpp \
    createpackdialog.cpp \
    texturepackfile.cpp \
    texturepacker.cpp \
    packcompare.cpp \
    packextractdialog.cpp \
    containeroverlayview.cpp \
    containeroverlayfile.cpp \
    containeroverlaydialog.cpp \
    tiledefcompare.cpp \
    checkbuildingswindow.cpp \
    checkmapswindow.cpp \
    rearrangetiles.cpp

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
    zgriditem.h \
    zprogress.h \
    minimap.h \
    convertorientationdialog.h \
    converttolotdialog.h \
    BuildingEditor/buildingeditorwindow.h \
    BuildingEditor/simplefile.h \
    BuildingEditor/buildingtools.h \
    BuildingEditor/buildingdocument.h \
    BuildingEditor/building.h \
    BuildingEditor/buildingfloor.h \
    BuildingEditor/buildingundoredo.h \
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
    BuildingEditor/templatefrombuildingdialog.h \
    BuildingEditor/buildingwriter.h \
    BuildingEditor/buildingreader.h \
    BuildingEditor/resizebuildingdialog.h \
    BuildingEditor/furnitureview.h \
    BuildingEditor/furnituregroups.h \
    BuildingEditor/buildingpreferences.h \
    BuildingEditor/buildingtmx.h \
    BuildingEditor/tilecategoryview.h \
    BuildingEditor/listofstringsdialog.h \
    tilemetainfodialog.h \
    tilemetainfomgr.h \
    BuildingEditor/horizontallinedelegate.h \
    BuildingEditor/buildingfloorsdialog.h \
    BuildingEditor/buildingtiletools.h \
    BuildingEditor/buildingmap.h \
    BuildingEditor/buildingfurnituredock.h \
    BuildingEditor/buildingtilesetdock.h \
    BuildingEditor/buildinglayersdock.h \
    tiledefdialog.h \
    tiledeffile.h \
    addtilesetsdialog.h \
    BuildingEditor/buildingorthoview.h \
    BuildingEditor/buildingisoview.h \
    BuildingEditor/choosetemplatesdialog.h \
    threads.h \
    BuildingEditor/buildingtileentryview.h \
    bmptool.h \
    bmpblender.h \
    bmptooldialog.h \
    bmpselectionitem.h \
    BuildingEditor/buildingpropertiesdialog.h \
    roomdefecator.h \
    tilelayerspanel.h \
    roomdeftool.h \
    roomdefnamedialog.h \
    bmpruleview.h \
    luatiled.h \
    luaconsole.h \
    worldeddock.h \
    worldlottool.h \
    BuildingEditor/buildingdocumentmgr.h \
    BuildingEditor/categorydock.h \
    BuildingEditor/imode.h \
    BuildingEditor/objecteditmode.h \
    BuildingEditor/tileeditmode.h \
    BuildingEditor/editmodestatusbar.h \
    BuildingEditor/objecteditmode_p.h \
    BuildingEditor/tileeditmode_p.h \
    BuildingEditor/embeddedmainwindow.h \
    BuildingEditor/singleton.h \
    BuildingEditor/fancytabwidget.h \
    BuildingEditor/utils/stylehelper.h \
    BuildingEditor/utils/styledbar.h \
    BuildingEditor/utils/hostosinfo.h \
    BuildingEditor/welcomemode.h \
    BuildingEditor/buildingroomdef.h \
    picktiletool.h \
    mapbuildings.h \
    bmpblendview.h \
    luamapsdialog.h \
    luaworlddialog.h \
    edgetool.h \
    edgetooldialog.h \
    curbtool.h \
    curbtooldialog.h \
    fencetool.h \
    fencetooldialog.h \
    luatiletool.h \
    luatooldialog.h \
    luatooloptions.h \
    virtualtilesetdialog.h \
    virtualtilesetview.h \
    virtualtileset.h \
    undoredobuttons.h \
    addvirtualtilesetdialog.h \
    tileshapeeditor.h \
    textureunpacker.h \
    texturemanager.h \
    addtexturesdialog.h \
    enflatulatordialog.h \
    texturepropertiesdialog.h \
    tileshapegroupdialog.h \
    tileshapegrouppropertiesdialog.h \
    tileshapepropertiesdialog.h \
    pixelbuffer.h \
    vtsprefsdialog.h \
    packviewer.h \
    createpackdialog.h \
    texturepackfile.h \
    texturepacker.h \
    packcompare.h \
    packextractdialog.h \
    containeroverlayview.h \
    containeroverlayfile.h \
    containeroverlaydialog.h \
    tiledefcompare.h \
    checkbuildingswindow.h \
    checkmapswindow.h \
    rearrangetiles.h

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
    BuildingEditor/newbuildingdialog.ui \
    BuildingEditor/buildingpreferencesdialog.ui \
    BuildingEditor/buildingtemplatesdialog.ui \
    BuildingEditor/choosebuildingtiledialog.ui \
    BuildingEditor/roomsdialog.ui \
    BuildingEditor/buildingtilesdialog.ui \
    BuildingEditor/templatefrombuildingdialog.ui \
    BuildingEditor/resizebuildingdialog.ui \
    BuildingEditor/listofstringsdialog.ui \
    tilemetainfodialog.ui \
    BuildingEditor/buildingfloorsdialog.ui \
    BuildingEditor/buildingtilesetdock.ui \
    BuildingEditor/buildinglayersdock.ui \
    tiledefdialog.ui \
    addtilesetsdialog.ui \
    BuildingEditor/choosetemplatesdialog.ui \
    bmptooldialog.ui \
    BuildingEditor/buildingpropertiesdialog.ui \
    roomdefnamedialog.ui \
    luaconsole.ui \
    worldeddock.ui \
    BuildingEditor/welcomemode.ui \
    luamapsdialog.ui \
    luaworlddialog.ui \
    edgetooldialog.ui \
    curbtooldialog.ui \
    fencetooldialog.ui \
    luatooldialog.ui \
    virtualtilesetdialog.ui \
    addvirtualtilesetdialog.ui \
    tileshapeeditor.ui \
    addtexturesdialog.ui \
    enflatulatordialog.ui \
    texturepropertiesdialog.ui \
    tileshapegroupdialog.ui \
    tileshapegrouppropertiesdialog.ui \
    tileshapepropertiesdialog.ui \
    vtsprefsdialog.ui \
    packviewer.ui \
    createpackdialog.ui \
    packcompare.ui \
    packextractdialog.ui \
    containeroverlaydialog.ui \
    tiledefcompare.ui \
    checkbuildingswindow.ui \
    checkmapswindow.ui \
    rearrangetiles.ui

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

OTHER_FILES += \
    luatiled.pkg


include(../tolua/src/lib/tolua.pri)
include(../lua/lua.pri)
TOLUA_PKGNAME = tiled
TOLUA_PKG = luatiled.pkg
TOLUA_DEPS = $$PWD/luatiled.h
include(../tolua/src/bin/tolua.pri)

isEmpty(INSTALL_ONLY_BUILD) {
    win32:CONFIG_PREFIX = $${target.path}
    unix:CONFIG_PREFIX = $${target.path}/../share/tilezed/config
    macx:CONFIG_PREFIX = $${target.path}/TileZed.app/Contents/Config

    win32:DOCS_PREFIX = $${target.path}/docs
    unix:DOCS_PREFIX = $${target.path}/../share/tilezed/docs
    macx:DOCS_PREFIX = $${target.path}/TileZed.app/Contents/Docs

    win32:LUA_PREFIX = $${target.path}/lua
    unix:LUA_PREFIX = $${target.path}/../share/tilezed/lua
    macx:LUA_PREFIX = $${target.path}/TileZed.app/Contents/Lua
} else {
    win32:CONFIG_PREFIX = $${top_builddir}
    unix:CONFIG_PREFIX = $${top_builddir}/share/tilezed/config
    macx:CONFIG_PREFIX = $${top_builddir}/bin/TileZed.app/Contents/Config

    win32:DOCS_PREFIX = $${top_builddir}/docs
    unix:DOCS_PREFIX = $${top_builddir}/share/tilezed/docs
    macx:DOCS_PREFIX = $${top_builddir}/TileZed.app/Contents/Docs

    win32:LUA_PREFIX = $${top_builddir}/lua
    unix:LUA_PREFIX = $${top_builddir}/share/tilezed/lua
    macx:LUA_PREFIX = $${top_builddir}/TileZed.app/Contents/Lua
}

configTxtFiles.path = $${CONFIG_PREFIX}
configTxtFiles.files = \
    $${top_srcdir}/LuaTools.txt \
    $${top_srcdir}/Textures.txt \
    $${top_srcdir}/TileProperties.txt \
    $${top_srcdir}/Tilesets.txt \
    $${top_srcdir}/TileShapes.txt
INSTALLS += configTxtFiles

buildingEdTxt.path = $${CONFIG_PREFIX}
buildingEdTxt.files = \
    BuildingEditor/BuildingFurniture.txt \
    BuildingEditor/BuildingTemplates.txt \
    BuildingEditor/BuildingTiles.txt \
    BuildingEditor/TMXConfig.txt
INSTALLS += buildingEdTxt

tiledDocs.path = $${DOCS_PREFIX}
tiledDocs.files = \
    $${top_srcdir}/docs/TileProperties \
    $${top_srcdir}/docs/TileZed
INSTALLS += tiledDocs

buildingEdDocs.path = $${DOCS_PREFIX}/BuildingEd
buildingEdDocs.files = \
    BuildingEditor/manual/*
INSTALLS += buildingEdDocs

luaScripts.path = $${LUA_PREFIX}
luaScripts.files = $${top_srcdir}/lua/*
INSTALLS += luaScripts

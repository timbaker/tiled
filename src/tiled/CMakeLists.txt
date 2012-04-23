project( TileZed )
cmake_minimum_required( VERSION 2.6 )
set ( CMAKE_BUILD_TYPE Release )
#add_definitions ( -Wall )
add_definitions( -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII )
find_package ( Qt4 REQUIRED )
include ( ${QT_USE_FILE} )
include_directories (
	../libtiled
	${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	${QT_QTOPENGL_INCLUDE_DIR} ${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR}
	)
#set ( EXECUTABLE_OUTPUT_PATH ../../bin )
set ( Tiled_HDRS
	addremovelayer.h
	addremovemapobject.h
	addremovetileset.h
	automapperwrapper.h
	automappingutils.h
	brushitem.h
	changemapobject.h
	changeimagelayerproperties.h
	changeobjectgroupproperties.h
	changepolygon.h
	changeproperties.h
	changetileselection.h
	commandlineparser.h
	erasetiles.h
	filltiles.h
	imagelayeritem.h
	languagemanager.h
	macsupport.h
	mapobjectitem.h
	mapreaderinterface.h
	mapwriterinterface.h
	movelayer.h
	movemapobject.h
	movemapobjecttogroup.h
	movetileset.h
	objectgroupitem.h
	objecttypes.h
	offsetlayer.h
	painttilelayer.h
	pluginmanager.h
	propertiesview.h
	rangeset.h
	renamelayer.h
	resizelayer.h
	resizemap.h
	resizemapobject.h
	selectionrectangle.h
	tilelayeritem.h
	tilepainter.h
	tmxmapreader.h
	tmxmapwriter.h
	undocommands.h
	utils.h

	zgriditem.hpp
	zlot.hpp
	ztilelayergroupitem.h
	)

set ( Tiled_SRCS
	aboutdialog.cpp
	abstractobjecttool.cpp
	abstracttiletool.cpp
	abstracttool.cpp
	addremovelayer.cpp
	addremovemapobject.cpp
	addremovetileset.cpp
	automapper.cpp
	automapperwrapper.cpp
	automappingmanager.cpp
	automappingutils.cpp
	brushitem.cpp
	bucketfilltool.cpp
	changemapobject.cpp
	changeimagelayerproperties.cpp
	changeobjectgroupproperties.cpp
	changepolygon.cpp
	changeproperties.cpp
	changetileselection.cpp
	clipboardmanager.cpp
	colorbutton.cpp
	commandbutton.cpp
	command.cpp
	commanddatamodel.cpp
	commanddialog.cpp
	commandlineparser.cpp
	createobjecttool.cpp
	documentmanager.cpp
	editpolygontool.cpp
	eraser.cpp
	erasetiles.cpp
	filesystemwatcher.cpp
	filltiles.cpp
	imagelayeritem.cpp
	imagelayerpropertiesdialog.cpp
	languagemanager.cpp
	layerdock.cpp
	layermodel.cpp
	main.cpp
	mainwindow.cpp
	mapdocumentactionhandler.cpp
	mapdocument.cpp
	mapobjectitem.cpp
	mapscene.cpp
	mapview.cpp
	movelayer.cpp
	movemapobject.cpp
	movemapobjecttogroup.cpp
	movetileset.cpp
	newmapdialog.cpp
	newtilesetdialog.cpp
	objectgroupitem.cpp
	objectgrouppropertiesdialog.cpp
	objectpropertiesdialog.cpp
	objectselectiontool.cpp
	objecttypes.cpp
	objecttypesmodel.cpp
	offsetlayer.cpp
	offsetmapdialog.cpp
	painttilelayer.cpp
	pluginmanager.cpp
	preferences.cpp
	preferencesdialog.cpp
	propertiesdialog.cpp
	propertiesmodel.cpp
	propertiesview.cpp
	quickstampmanager.cpp
	renamelayer.cpp
	resizedialog.cpp
	resizehelper.cpp
	resizelayer.cpp
	resizemap.cpp
	resizemapobject.cpp
	saveasimagedialog.cpp
	selectionrectangle.cpp
	stampbrush.cpp
	tiledapplication.cpp
	tilelayeritem.cpp
	tilepainter.cpp
	tileselectionitem.cpp
	tileselectiontool.cpp
	tilesetdock.cpp
	tilesetmanager.cpp
	tilesetmodel.cpp
	tilesetview.cpp
	tmxmapreader.cpp
	tmxmapwriter.cpp
	toolmanager.cpp
	undodock.cpp
	utils.cpp
	zoomable.cpp

	zgriditem.cpp
	zlevelsdock.cpp
	zlevelsmodel.cpp
	zlot.cpp
	zlotmanager.cpp
	zmapobjectmodel.cpp
	zmapsdock.cpp
	zobjectsdock.cpp
	ZomboidScene.cpp
	zprogress.cpp
	ztilelayergroupitem.cpp
	ztilesetthumbmodel.cpp
	ztilesetthumbview.cpp
	)

set ( Tiled_UIS
	aboutdialog.ui
	commanddialog.ui
	mainwindow.ui
	newmapdialog.ui
	newtilesetdialog.ui
	objectpropertiesdialog.ui
	offsetmapdialog.ui
	preferencesdialog.ui
	propertiesdialog.ui
	resizedialog.ui
	saveasimagedialog.ui
	newimagelayerdialog.ui
	)
QT4_WRAP_UI(UIS ${Tiled_UIS})

set ( Tiled_RSCS
	tiled.qrc
	)
QT4_ADD_RESOURCES(RSCS ${Tiled_RSCS})

set ( Tiled_MOCS
	aboutdialog.h
	abstractobjecttool.h
	abstracttiletool.h
	abstracttool.h
	automapper.h
	automappingmanager.h
	bucketfilltool.h
	clipboardmanager.h
	colorbutton.h
	commandbutton.h
	commanddatamodel.h
	commanddialog.h
	command.h
	createobjecttool.h
	documentmanager.h
	editpolygontool.h
	eraser.h
	filesystemwatcher.h
	imagelayerpropertiesdialog.h
	layerdock.h
	layermodel.h
	mainwindow.h
	mapdocumentactionhandler.h
	mapdocument.h
	mapscene.h
	mapview.h
	newmapdialog.h
	newtilesetdialog.h
	objectgrouppropertiesdialog.h
	objectpropertiesdialog.h
	objectselectiontool.h
	objecttypesmodel.h
	offsetmapdialog.h
	preferencesdialog.h
	preferences.h
	propertiesdialog.h
	propertiesmodel.h
	quickstampmanager.h
	resizedialog.h
	resizehelper.h
	saveasimagedialog.h
	stampbrush.h
	tiledapplication.h
	tileselectionitem.h
	tileselectiontool.h
	tilesetdock.h
	tilesetmanager.h
	tilesetmodel.h
	tilesetview.h
	toolmanager.h
	undodock.h
	zoomable.h

	zlevelsdock.hpp
	zlevelsmodel.hpp
	zlotmanager.hpp
	zmapobjectmodel.hpp
	zmapsdock.hpp
	ZomboidScene.h
	zobjectsdock.hpp
	zprogress.hpp
	ztilesetthumbmodel.hpp
	ztilesetthumbview.hpp
	)
QT4_WRAP_CPP(MOCS ${Tiled_MOCS})

QT4_GET_MOC_FLAGS(_moc_INCS)
GET_FILENAME_COMPONENT(_abs_FILE toolmanager.cpp ABSOLUTE)
#GET_FILENAME_COMPONENT(_abs_PATH ${_abs_FILE} PATH)
set(_current_MOC toolmanager.moc)
SET(_moc    ${CMAKE_CURRENT_BINARY_DIR}/${_current_MOC})
QT4_CREATE_MOC_COMMAND(${_abs_FILE} ${_moc} "${_moc_INCS}" "")
MACRO_ADD_FILE_DEPENDENCIES(${_abs_FILE} ${_moc})

set(ALL_HDRS ${Tiled_HDRS} ${Tiled_MOCS})

add_executable ( TileZed ${Tiled_SRCS} tiled.rc ${UIS} ${RSCS} ${TRS} ${MOCS} ${ALL_HDRS})
target_link_libraries ( TileZed libtiled ${QT_QTOPENGL_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} )

INSTALL(TARGETS TileZed DESTINATION ".")

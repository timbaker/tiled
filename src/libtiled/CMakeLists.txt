project( libtiled )
cmake_minimum_required( VERSION 2.6 )
set ( CMAKE_BUILD_TYPE Release )
#add_definitions ( -Wall )
add_definitions( -DTILED_LIBRARY )
find_package ( Qt4 REQUIRED )
include ( ${QT_USE_FILE} )
include_directories (
	${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR}
	../zlib)

set ( tiled_HDRS
	compression.h
	imagelayer.h
	isometricrenderer.h
	layer.h
	map.h
	mapobject.h
	mapreader.h
	maprenderer.h
	mapwriter.h
	object.h
	objectgroup.h
	orthogonalrenderer.h
	properties.h
	staggeredrenderer.h
	tile.h
	tiled_global.h
	tilelayer.h
	tileset.h
	gidmapper.h

	pathlayer.h
	pathgenerator.h
	zlevelrenderer.h
	ztilelayergroup.h
	)

set ( tiled_SRCS
	compression.cpp
	imagelayer.cpp
	isometricrenderer.cpp
	layer.cpp
	map.cpp
	mapobject.cpp
	mapreader.cpp
	maprenderer.cpp
	mapwriter.cpp
	objectgroup.cpp
	orthogonalrenderer.cpp
	properties.cpp
	staggeredrenderer.cpp
	tilelayer.cpp
	tileset.cpp
	gidmapper.cpp

	pathlayer.cpp;
	pathgenerator.cpp
	zlevelrenderer.cpp
	ztilelayergroup.cpp
	)

add_library ( libtiled SHARED ${tiled_SRCS} ${tiled_HDRS} ${UIS} ${RSCS} ${TRS} ${MOCS} )
target_link_libraries ( libtiled  ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} )

INSTALL(TARGETS libtiled RUNTIME DESTINATION "." LIBRARY DESTINATION ".")

cmake_minimum_required( VERSION 2.6 )
set ( CMAKE_BUILD_TYPE Release )
#add_definitions ( -Wall )
add_definitions( -DREPLICAISLAND_LIBRARY )
find_package ( Qt4 REQUIRED )
include ( ${QT_USE_FILE} )
include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/../../tiled
	${CMAKE_CURRENT_SOURCE_DIR}/../../libtiled
	${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR}
	)
set ( replicaisland_HDRS
	replicaisland_global.h
	)

set ( replicaisland_SRCS
	replicaislandplugin.cpp
	)

set ( replicaisland_RSCS
	replicaisland.qrc
	)
QT4_ADD_RESOURCES(RSCS ${replicaisland_RSCS})

set ( replicaisland_MOCS
	replicaislandplugin.h
	)
QT4_WRAP_CPP(MOCS ${replicaisland_MOCS})

add_library ( replicaisland SHARED ${replicaisland_SRCS} ${UIS} ${RSCS} ${TRS} ${MOCS} )
target_link_libraries ( replicaisland libtiled ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} )

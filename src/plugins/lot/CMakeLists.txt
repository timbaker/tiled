cmake_minimum_required( VERSION 2.6 )
set ( CMAKE_BUILD_TYPE Release )
#add_definitions ( -Wall )
add_definitions(-DLOT_LIBRARY ${QT_DEFINITIONS} -DQT_PLUGIN -DQT_SHARED)
find_package ( Qt4 REQUIRED )
include ( ${QT_USE_FILE} )
include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/../../tiled
	${CMAKE_CURRENT_SOURCE_DIR}/../../libtiled
	${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR}
	)
set ( lot_HDRS
	lot_global.h
	)

set ( lot_SRCS
	lotplugin.cpp
	)

set ( lot_MOCS
	lotplugin.h
	)
QT4_WRAP_CPP(MOCS ${lot_MOCS})

add_library ( lot SHARED ${lot_SRCS} ${lot_HDRS} ${lot_MOCS} ${UIS} ${RSCS} ${TRS} ${MOCS} )
target_link_libraries ( lot libtiled ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} )

INSTALL(TARGETS lot RUNTIME DESTINATION plugins/tiled LIBRARY DESTINATION plugins/tiled)

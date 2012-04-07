cmake_minimum_required( VERSION 2.6 )
set ( CMAKE_BUILD_TYPE Release )
#add_definitions ( -Wall )
add_definitions(-DDROIDCRAFT_LIBRARY ${QT_DEFINITIONS} -DQT_PLUGIN -DQT_SHARED)
find_package ( Qt4 REQUIRED )
include ( ${QT_USE_FILE} )
include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/../../tiled
	${CMAKE_CURRENT_SOURCE_DIR}/../../libtiled
	${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR}
	)
set ( droidcraft_HDRS
	droidcraft_global.h
	)

set ( droidcraft_SRCS
	droidcraftplugin.cpp
	)

set ( droidcraft_RSCS
	droidcraft.qrc
	)
QT4_ADD_RESOURCES(RSCS ${droidcraft_RSCS})

set ( droidcraft_MOCS
	droidcraftplugin.h
	)
QT4_WRAP_CPP(MOCS ${droidcraft_MOCS})

add_library ( droidcraft SHARED ${droidcraft_SRCS} ${UIS} ${RSCS} ${TRS} ${MOCS} )
target_link_libraries ( droidcraft libtiled ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} )

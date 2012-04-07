cmake_minimum_required( VERSION 2.6 )
set ( CMAKE_BUILD_TYPE Release )
#add_definitions ( -Wall )
add_definitions(-DFLARE_LIBRARY ${QT_DEFINITIONS} -DQT_PLUGIN -DQT_SHARED)
find_package ( Qt4 REQUIRED )
include ( ${QT_USE_FILE} )
include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/../../tiled
	${CMAKE_CURRENT_SOURCE_DIR}/../../libtiled
	${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR}
	)
set ( flare_HDRS
	flare_global.h
	)

set ( flare_SRCS
	flareplugin.cpp
	)

set ( flare_MOCS
	flareplugin.h
	)
QT4_WRAP_CPP(MOCS ${flare_MOCS})

add_library ( flare SHARED ${flare_SRCS} ${UIS} ${RSCS} ${TRS} ${MOCS} )
target_link_libraries ( flare libtiled ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} )


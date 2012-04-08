cmake_minimum_required( VERSION 2.6 )
set ( CMAKE_BUILD_TYPE Release )
#add_definitions ( -Wall )
add_definitions( -DJSON_LIBRARY )
find_package ( Qt4 REQUIRED )
include ( ${QT_USE_FILE} )
include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/../../tiled
	${CMAKE_CURRENT_SOURCE_DIR}/../../libtiled
	${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR}
	)
set ( json_HDRS
	json_global.h
	qjsonparser/json.h
	varianttomapconverter.h
	maptovariantconverter.h
	)

set ( json_SRCS
	jsonplugin.cpp
	qjsonparser/json.cpp
	varianttomapconverter.cpp
	maptovariantconverter.cpp
	)

set ( json_MOCS
	jsonplugin.h
	)
QT4_WRAP_CPP(MOCS ${json_MOCS})

add_library ( json SHARED ${json_SRCS} ${UIS} ${RSCS} ${TRS} ${MOCS} )
target_link_libraries ( json libtiled ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} )

INSTALL(TARGETS json RUNTIME DESTINATION plugins/tiled LIBRARY DESTINATION plugins/tiled)

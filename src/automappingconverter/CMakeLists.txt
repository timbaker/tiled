project( automappingconverter )
cmake_minimum_required( VERSION 2.6 )
set ( CMAKE_BUILD_TYPE Release )
#add_definitions ( -Wall )
find_package ( Qt4 REQUIRED )
include ( ${QT_USE_FILE} )
include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/../libtiled
	${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR} ${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR}
	)
#set ( EXECUTABLE_OUTPUT_PATH ../../bin )
set ( automappingconverter_HDRS
	convertercontrol.h
	)

set ( automappingconverter_SRCS
	main.cpp
	converterdatamodel.cpp
	convertercontrol.cpp
	converterwindow.cpp
	)

set ( automappingconverter_UIS
	converterwindow.ui
	)
QT4_WRAP_UI(UIS ${automappingconverter_UIS})

set ( automappingconverter_MOCS
	converterdatamodel.h
	converterwindow.h
	)
QT4_WRAP_CPP(MOCS ${automappingconverter_MOCS})

add_executable ( automappingconverter ${automappingconverter_SRCS} ${UIS} ${RSCS} ${TRS} ${MOCS} )
target_link_libraries ( automappingconverter libtiled ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} )
